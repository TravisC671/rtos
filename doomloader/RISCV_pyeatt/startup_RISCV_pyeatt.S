	/* Bare-metal startup for RISCV_pyeatt
	*
	* The processor always uses vectored interrupt mode.  In vectored
	* mode the base address (mtvec[31:2]<<2) is used for synchronous
	* exceptions, and base + 4*cause is used for asynchronous
	* interrupts.  Because the hardware reset entry is at PC=0 and must
	* contain the boot bootstrap, the trap vector table cannot also be
	* placed at address 0 -- its entry 0 would collide with the reset
	* jump and every ECALL (FreeRTOS yield) would reboot the CPU.
	*
	* Layout:
	*   .reset_entry   at 0x00000000 : j _Reset_Handler  (power-on entry)
	*   .vectors       (128-byte aligned, after .reset_entry)
	*                  entry 0  = synchronous exception / ECALL
	*                  entry 7  = machine timer interrupt
	*                  entry 18 = platform IRQ[2] (AXI timer 0)
	*   _Reset_Handler loads mtvec with __Vectors | 1 (vectored mode).
	*/

	/* =================================================================
	* Hardware reset bootstrap at address 0
	* =============================================================== */
	.section .reset_entry, "ax", @progbits
	.align  2
	.globl  _start_reset
_start_reset:
	j       _Reset_Handler

	/* =================================================================
	* Vectored interrupt table (32 entries, 128-byte aligned)
	* =============================================================== */

	.section .vectors, "ax", @progbits
	.align  7                       /* 128-byte alignment */
	.globl  __Vectors
	.globl  __Vectors_End
	.globl  __Vectors_Size

	.extern freertos_risc_v_exception_handler

__Vectors:
	/* 0x00: synchronous exceptions (illegal insn, misaligned,
	 * ECALL from M-mode, etc.).  The FreeRTOS port provides a
	 * handler that recognises ECALL and performs the context
	 * switch, and forwards everything else to
	 * freertos_risc_v_application_exception_handler. */
	j       freertos_risc_v_exception_handler
	j       _unhandled      /* 0x04: reserved (cause 1) */
	j       _unhandled      /* 0x08: reserved (cause 2) */
	j       _unhandled      /* 0x0C: MSI */
	j       _unhandled      /* 0x10: reserved (cause 4) */
	j       _unhandled      /* 0x14: reserved (cause 5) */
	j       _unhandled      /* 0x18: reserved (cause 6) */
	/* HARDWARE-BUG WORKAROUND: vectored dispatch can land an MTI
	 * (cause 7) at base+4*8 instead of base+4*7 when the CPU is
	 * stalled on a long AXI store at the moment the timer fires.
	 * Mirroring MTI at slot 8 catches the off-by-one so the timer
	 * handler runs either way; mcause still reads 7, so the handler
	 * itself is unaffected.  Cause 8 is U-mode ECALL which can't
	 * occur on an M-only design, so the slot is safe to repurpose.
	 * Remove once the RTL race is fixed. */
	/* 0x1C: MTI - machine timer interrupt */
	j       freertos_risc_v_mtimer_interrupt_handler
	/* 0x20: cause 8 mirror - MTI off-by-one workaround */
	j       freertos_risc_v_mtimer_interrupt_handler
	j       _unhandled      /* 0x24: reserved (cause 9) */
	j       _unhandled      /* 0x28: reserved (cause 10) */
	j       _unhandled      /* 0x2C: MEI - machine external interrupt */
	j       _unhandled      /* 0x30: reserved (cause 12) */
	j       _unhandled      /* 0x34: reserved (cause 13) */
	j       _unhandled      /* 0x38: reserved (cause 14) */
	j       _unhandled      /* 0x3C: reserved (cause 15) */
	j       UART0_handler   /* 0x40: platform IRQ[0] UART 0 */
	j       UART1_handler   /* 0x44: platform IRQ[1] UART 1 */
	j       AXI_TIMER_0_ISR /* 0x48: platform IRQ[2] AXI Timer 0 */
	j       _unhandled      /* 0x4C: platform IRQ[3] AXI Timer 1 */
	j       _unhandled      /* 0x50: platform IRQ[4] GPIO 0 */
	j       _unhandled      /* 0x54: platform IRQ[5] GPIO 1 */
	j       _unhandled      /* 0x58: platform IRQ[6] GPIO 2 */
	j       _unhandled      /* 0x5C: platform IRQ[7]
	j       SD_handler      /* 0x60: platform IRQ[8] SD controller */
	j       _unhandled      /* 0x64: platform IRQ[9] VGA controller */
	j       _unhandled      /* 0x68: platform IRQ[10] */
	j       _unhandled      /* 0x6C: platform IRQ[11] */
	j       _unhandled      /* 0x70: platform IRQ[12] */
	j       _unhandled      /* 0x74: platform IRQ[13] */
	j       _unhandled      /* 0x78: platform IRQ[14] */
	j       _unhandled      /* 0x7C: platform IRQ[15] */

	/* =================================================================
	* Main startup
	* =============================================================== */
	.section .text.init, "ax"
	.globl _start

       .section .resetHandler
        .func    _Reset_Handler
        .global  _Reset_Handler
_Reset_Handler:
	/* Clear any pending interrupts and disable all interrupt sources.
	* On a second run (after ebreak restart), stale pending bits from
	* timers or external sources can cause immediate re-entry into
	* _unhandled. */
	csrw    mie, zero           /* disable all interrupt sources */
	csrci   mstatus, 0x8        /* clear MIE (global interrupt enable) */
	csrw    mip, zero           /* clear pending interrupt bits */

	/* Point mtvec at the relocated vector table.  Vectored mode
	* (bit 0 = 1) is required so asynchronous interrupts dispatch
	* through the per-cause entries and synchronous exceptions go
	* to entry 0 (freertos_risc_v_exception_handler). */
	la      t0, __Vectors
	ori     t0, t0, 1
	csrw    mtvec, t0

	/* Initialize processor registers to 0 */
        addi    x1,x0,0
        addi    x3,x0,0
        addi    x4,x0,0
        addi    x5,x0,0
        addi    x6,x0,0
        addi    x7,x0,0
        addi    x8,x0,0
        addi    x9,x0,0
        addi    x10,x0,0
        addi    x11,x0,0
        addi    x12,x0,0
        addi    x13,x0,0
        addi    x14,x0,0
        addi    x15,x0,0
        addi    x16,x0,0
        addi    x17,x0,0
        addi    x18,x0,0
        addi    x19,x0,0
        addi    x20,x0,0
        addi    x21,x0,0
        addi    x22,x0,0
        addi    x23,x0,0
        addi    x24,x0,0
        addi    x25,x0,0
        addi    x26,x0,0
        addi    x27,x0,0
        addi    x28,x0,0
        addi    x29,x0,0
        addi    x30,x0,0
        addi    x31,x0,0

	/* Load the stack pointer */
	la      sp, __StackTop

       // clear the bss section
        la      t0,__bss_start__ // get address where .bss starts
        la      t1,__bss_end__   // get address where .bss ends
        bge     t0,t1,movedata   // if there is no .bss, skip the loop
        mv      t3,zero
bssloop:sw      t3,(t0)
        addi    t0,t0,4
        blt     t0,t1,bssloop

        // copy initialized data from ROM to RAM
movedata:
        la     t0,__copy_table_start__  
        la     t1,__copy_table_end__
copyloop:
        bge     t0,t1,call_main // exit if not more entries in the table
        lw      t2,(t0)       // get source address of next chunk
        addi    t0,t0,4
        lw      t4,(t0)       // get target address of next chunk
        addi    t0,t0,4
        lw      t3,(t0)       // get number of words to move
        addi    t0,t0,4
        beqz    t3,copyloop
ctloop: lw      t5,(t2)      // load word from this chunk
        addi    t2,t2,4
        sw      t5,(t4)      // store word from this chunk to the target addr
        addi    t4,t4,4
        addi    t3,t3,-1     // subtract 
        bgtz    t3,ctloop    // if not done, keep going.
        j       copyloop     // top of loop checks to see if we are done.

	/* fence
	fence.i  */

call_main:
        jal   main
hang:   j     hang
        .endfunc

_unhandled:
	ebreak                  /* DIAG: catch any unhandled vector dispatch */
	j       _unhandled
