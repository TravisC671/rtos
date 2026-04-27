/* irq.S - Interrupt control routines for the RISC-V Pyeatt core (M-mode).
 *
 * Conventions: RV32I calling convention.  All routines preserve the
 * callee-saved registers and touch only a0 (the return/argument register)
 * plus the CSRs noted below.
 *
 * CSRs used:
 *   mstatus bit 3 (MIE) -- global M-mode interrupt enable.
 *   mie                 -- per-source enable mask (bits 3, 7, 11, 16..).
 *   mip                 -- per-source pending mask.  Read-only
 *                          passthrough for the standard and platform
 *                          sources; clear the source device to clear
 *                          the pending bit.
 */

    .section .text
    .align 2

/* ---------------------------------------------------------------------
 * void irq_global_enable(void);
 *
 * Set mstatus.MIE so the core will accept interrupts.
 * ------------------------------------------------------------------- */
    .globl  irq_global_enable
irq_global_enable:
    csrsi   mstatus, 0x8
    ret

/* ---------------------------------------------------------------------
 * uint32_t irq_global_disable(void);
 *
 * Clear mstatus.MIE and return the previous value (0 or 1) so the
 * caller can restore it after a critical section.
 * ------------------------------------------------------------------- */
    .globl  irq_global_disable
irq_global_disable:
    csrrci  a0, mstatus, 0x8
    srli    a0, a0, 3
    andi    a0, a0, 1
    ret

/* ---------------------------------------------------------------------
 * void irq_global_restore(uint32_t prev);
 *
 * Restore mstatus.MIE to the value returned by irq_global_disable.
 * a0 == 0 leaves MIE cleared; any non-zero value sets MIE.
 * ------------------------------------------------------------------- */
    .globl  irq_global_restore
irq_global_restore:
    beqz    a0, 1f
    csrsi   mstatus, 0x8
    ret
1:
    csrci   mstatus, 0x8
    ret

/* ---------------------------------------------------------------------
 * uint32_t irq_global_status(void);
 *
 * Return the current value of mstatus.MIE (0 or 1).
 * ------------------------------------------------------------------- */
    .globl  irq_global_status
irq_global_status:
    csrr    a0, mstatus
    srli    a0, a0, 3
    andi    a0, a0, 1
    ret

/* ---------------------------------------------------------------------
 * void irq_enable(uint32_t mask);
 *
 * Set the bits in mask within the mie CSR, enabling those interrupt
 * sources.  Mask bit positions follow the mip/mie layout: 3 = MSI,
 * 7 = MTI, 11 = MEI, 16+n = platform irq[n].
 * ------------------------------------------------------------------- */
    .globl  irq_enable
irq_enable:
    csrs    mie, a0
    ret

/* ---------------------------------------------------------------------
 * void irq_disable(uint32_t mask);
 *
 * Clear the bits in mask within the mie CSR, masking those sources.
 * ------------------------------------------------------------------- */
    .globl  irq_disable
irq_disable:
    csrc    mie, a0
    ret

/* ---------------------------------------------------------------------
 * uint32_t irq_enabled_mask(void);
 *
 * Return the current contents of mie.
 * ------------------------------------------------------------------- */
    .globl  irq_enabled_mask
irq_enabled_mask:
    csrr    a0, mie
    ret

/* ---------------------------------------------------------------------
 * uint32_t irq_pending_mask(void);
 *
 * Return the current contents of mip.  The standard bits (MSIP, MTIP,
 * MEIP) and the platform bits are read-only and reflect the level on
 * the corresponding top-level input port; to clear a pending bit,
 * clear the source device (write MTIMER mtimecmp, clear the MSIP word,
 * service the PLIC/UART, etc.).
 * ------------------------------------------------------------------- */
    .globl  irq_pending_mask
irq_pending_mask:
    csrr    a0, mip
    ret
