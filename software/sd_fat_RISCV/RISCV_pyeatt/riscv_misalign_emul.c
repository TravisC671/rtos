/*
 * RISCV_pyeatt synchronous-exception handler dispatcher and the
 * built-in misaligned load/store emulator.
 *
 * The companion asm shim (riscv_misalign_shim.S) overrides FreeRTOS's
 * weak freertos_risc_v_application_exception_handler symbol.  When
 * the core raises a synchronous exception that the FreeRTOS port did
 * not consume (everything except mcause=11 ECALL, which the port
 * routes to vTaskSwitchContext), the shim recovers the saved register
 * frame from pxCurrentTCB->pxTopOfStack, computes the original
 * pre-trap sp, and calls riscv_trap_handler() with those plus mcause
 * and the original mepc.
 *
 * riscv_trap_handler() is the single point of generalisation.  It
 * dispatches by mcause:
 *
 *   2          -> riscv_trap_illegal_instruction()  (weak hook)
 *   3          -> riscv_trap_breakpoint()           (weak hook)
 *   4 or 6     -> riscv_emul_misalign()             (built-in)
 *   5 or 7     -> riscv_trap_access_fault()         (weak hook)
 *   anything else -> not handled; shim ebreaks for the debugger.
 *
 * Each weak hook returns 0 by default, which means "not handled" and
 * causes the shim to fall through to the same ebreak diagnostic the
 * FreeRTOS port uses when nothing wanted the trap.  An application
 * that wants to deal with a particular cause provides its own strong
 * definition of the corresponding hook; the linker then prefers the
 * application's version and the trap is handled there.
 *
 * This file is compiled with -mstrict-align (set per-file in
 * CMakeLists.txt) so the misalign emulator itself never causes a
 * recursive trap.
 *
 * Frame layout matches portcontextSAVE_CONTEXT_INTERNAL in
 * /opt/packages/FreeRTOS-Kernel/portable/GCC/RISC-V/portContext.h:
 *   frame[0]  = mepc + 4 (the address mret will return to)
 *   frame[1]  = x1 (ra)
 *   frame[2]  = x5
 *   frame[3]  = x6
 *   ...
 *   frame[28] = x31
 *   frame[29] = critical nesting
 *   frame[30] = mstatus
 *
 * x2 (sp), x3 (gp), x4 (tp) are NOT in the frame.
 *   x2: original_sp = (uint32_t)frame + portCONTEXT_SIZE.  The shim
 *       computes this and passes it in as orig_sp.
 *   x3, x4: not handled.  The compiler should not emit a load/store
 *       with gp or tp as the base register; if one ever appears, the
 *       emulator returns 0 and the access uses the wrong address.
 */

#include <stdint.h>

/* ====================================================================
 * Weak hooks for application-defined trap handlers
 *
 * Each returns 0 by default.  Override in user code to handle the
 * corresponding cause; return 1 if you consume the trap, 0 to fall
 * through to the shim's ebreak diagnostic.
 * ==================================================================== */

__attribute__((weak))
int riscv_trap_illegal_instruction(uint32_t  mepc,
                                   uint32_t *frame,
                                   uint32_t  orig_sp)
{
    (void)mepc;
    (void)frame;
    (void)orig_sp;
    return 0;
}

__attribute__((weak))
int riscv_trap_breakpoint(uint32_t  mepc,
                          uint32_t *frame,
                          uint32_t  orig_sp)
{
    (void)mepc;
    (void)frame;
    (void)orig_sp;
    return 0;
}

__attribute__((weak))
int riscv_trap_access_fault(uint32_t  mcause,
                            uint32_t  mepc,
                            uint32_t *frame,
                            uint32_t  orig_sp)
{
    (void)mcause;
    (void)mepc;
    (void)frame;
    (void)orig_sp;
    return 0;
}

/* ====================================================================
 * Misalign emulator helpers (read/write a saved register slot)
 * ==================================================================== */

static uint32_t read_reg(const uint32_t *frame,
                         uint32_t        reg,
                         uint32_t        orig_sp)
{
    if (reg == 0u) {
        return 0u;
    }
    if (reg == 1u) {
        return frame[1];
    }
    if (reg == 2u) {
        return orig_sp;
    }
    if (reg >= 5u && reg <= 31u) {
        return frame[reg - 3u];
    }
    /* x3 (gp), x4 (tp): not saved, not expected. */
    return 0u;
}

static void write_reg(uint32_t *frame,
                      uint32_t  reg,
                      uint32_t  value)
{
    if (reg == 0u) {
        return;
    }
    if (reg == 1u) {
        frame[1] = value;
        return;
    }
    if (reg >= 5u && reg <= 31u) {
        frame[reg - 3u] = value;
    }
    /* x2/x3/x4 are not destinations of compiler-emitted loads. */
}

/* ====================================================================
 * Misaligned load/store emulator (mcause 4 or 6)
 * ==================================================================== */

int riscv_emul_misalign(uint32_t  mcause,
                        uint32_t  mepc,
                        uint32_t *frame,
                        uint32_t  orig_sp)
{
    uint32_t       insn;
    uint32_t       opcode;
    uint32_t       funct3;
    uint32_t       rd;
    uint32_t       rs1;
    uint32_t       rs2;
    int32_t        imm;
    uint32_t       addr;
    uint32_t       value;
    const uint8_t *src;
    uint8_t       *dst;
    int            handled;

    (void)mcause;
    handled = 1;

    /* mepc is always 4-byte aligned, so this load itself never
     * recursively misaligns. */
    insn   = *(const uint32_t *)mepc;
    opcode = insn & 0x7Fu;
    rd     = (insn >> 7)  & 0x1Fu;
    funct3 = (insn >> 12) & 0x07u;
    rs1    = (insn >> 15) & 0x1Fu;
    rs2    = (insn >> 20) & 0x1Fu;

    if (opcode == 0x03u) {
        /* I-type load: imm[11:0] = insn[31:20], sign-extended. */
        imm   = (int32_t)insn >> 20;
        addr  = read_reg(frame, rs1, orig_sp) + (uint32_t)imm;
        src   = (const uint8_t *)addr;
        value = 0u;
        switch (funct3) {
            case 0u:
                /* LB: sign-extend 8 -> 32. */
                value = (uint32_t)(int32_t)(int8_t)src[0];
                break;
            case 1u:
                /* LH: sign-extend 16 -> 32. */
                value =  (uint32_t)src[0]
                      | ((uint32_t)src[1] << 8);
                value = (uint32_t)(int32_t)(int16_t)value;
                break;
            case 2u:
                /* LW. */
                value =  (uint32_t)src[0]
                      | ((uint32_t)src[1] << 8)
                      | ((uint32_t)src[2] << 16)
                      | ((uint32_t)src[3] << 24);
                break;
            case 4u:
                /* LBU. */
                value = (uint32_t)src[0];
                break;
            case 5u:
                /* LHU. */
                value =  (uint32_t)src[0]
                      | ((uint32_t)src[1] << 8);
                break;
            default:
                handled = 0;
                break;
        }
        if (handled != 0) {
            write_reg(frame, rd, value);
        }
    } else if (opcode == 0x23u) {
        /* S-type store: imm[11:5]=insn[31:25], imm[4:0]=insn[11:7].
         * Sign-extend by treating the high 7 bits as a signed value
         * shifted into place. */
        imm   = ((int32_t)(insn & 0xFE000000u) >> 20)
              | ((int32_t)((insn >> 7) & 0x1Fu));
        addr  = read_reg(frame, rs1, orig_sp) + (uint32_t)imm;
        value = read_reg(frame, rs2, orig_sp);
        dst   = (uint8_t *)addr;
        switch (funct3) {
            case 0u:
                /* SB. */
                dst[0] = (uint8_t)value;
                break;
            case 1u:
                /* SH. */
                dst[0] = (uint8_t)value;
                dst[1] = (uint8_t)(value >> 8);
                break;
            case 2u:
                /* SW. */
                dst[0] = (uint8_t)value;
                dst[1] = (uint8_t)(value >> 8);
                dst[2] = (uint8_t)(value >> 16);
                dst[3] = (uint8_t)(value >> 24);
                break;
            default:
                handled = 0;
                break;
        }
    } else {
        /* AMO (0x2F), FP load (0x07), FP store (0x27), or anything
         * else is not a normal scalar load/store.  Bail to ebreak. */
        handled = 0;
    }

    return handled;
}

/* ====================================================================
 * Top-level dispatcher
 *
 * Called from the asm shim with the registered frame pointer and the
 * original sp.  Returns 1 if the trap was consumed, 0 to fall
 * through to ebreak.
 * ==================================================================== */

int riscv_trap_handler(uint32_t  mcause,
                       uint32_t  mepc,
                       uint32_t *frame,
                       uint32_t  orig_sp)
{
    int rc;

    switch (mcause) {
        case 2u:
            /* illegal instruction */
            rc = riscv_trap_illegal_instruction(mepc, frame, orig_sp);
            break;
        case 3u:
            /* breakpoint */
            rc = riscv_trap_breakpoint(mepc, frame, orig_sp);
            break;
        case 4u:
        case 6u:
            /* load / store address misaligned */
            rc = riscv_emul_misalign(mcause, mepc, frame, orig_sp);
            break;
        case 5u:
        case 7u:
            /* load / store access fault */
            rc = riscv_trap_access_fault(mcause, mepc, frame, orig_sp);
            break;
        default:
            /* mcause 0 (insn address misaligned), 1 (insn access
             * fault), or anything else this dispatcher does not
             * recognise -- not handled. */
            rc = 0;
            break;
    }
    return rc;
}
