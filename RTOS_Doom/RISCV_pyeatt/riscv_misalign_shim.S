/* *********************************************************
 * Name:    riscv_misalign_shim.S
 * Author:  Larry Pyeatt
 * Date:    4/27/2026
 *
 * Strong override of the FreeRTOS RISC-V port's weak
 * freertos_risc_v_application_exception_handler symbol.  Builds
 * the standard four-argument call into the C trap dispatcher and
 * falls through to an ebreak diagnostic if the dispatcher does
 * not consume the trap.
 *
 * On entry from freertos_risc_v_exception_handler:
 *     a0 = mcause
 *     a1 = mepc + 4   (already advanced by the FreeRTOS port's
 *                      portcontextSAVE_EXCEPTION_CONTEXT macro)
 *     sp = ISR stack
 *   The saved task register frame is at *(uint32_t **)pxCurrentTCB.
 *
 * On exit:
 *   Returns to the caller (the FreeRTOS exception_handler) which
 *   then calls portcontextRESTORE_CONTEXT and mret.  The saved
 *   frame slot 0 already holds mepc + 4, so mret skips the
 *   faulting instruction.  When the C dispatcher does not consume
 *   the trap we never return to the caller; we ebreak instead.
 * ********************************************************* */

/* portCONTEXT_SIZE mirrors the value defined for RV32 non-E in
 * /opt/packages/FreeRTOS-Kernel/portable/GCC/RISC-V/portContext.h
 * (63 words = 252 bytes).  Used to compute the original pre-trap
 * sp from the saved frame pointer because FreeRTOS does not save
 * x2 (sp) in the frame.  When FreeRTOS upstream changes the frame
 * size, this constant must follow. */
	.equ	portCONTEXT_SIZE, 63 * 4

	.section .text
	.align	2

	/* *****************************************************
	 * freertos_risc_v_application_exception_handler:
	 *   Asm shim that prepares arguments and calls the C
	 *   dispatcher riscv_trap_handler().  The dispatcher
	 *   returns 1 in a0 when it consumes the trap, 0 to fall
	 *   through to the local ebreak diagnostic.
	 * ***************************************************** */
	.global	freertos_risc_v_application_exception_handler
	.extern	pxCurrentTCB
	.extern	riscv_trap_handler
freertos_risc_v_application_exception_handler:
	addi	sp, sp, -16		/* allocate a 16-byte frame */
	sw	ra, 12(sp)		/* save return address */

	/* Load saved task frame pointer.  pxCurrentTCB is the
	 * global pointer to the running TCB; pxTopOfStack is the
	 * first member, hence two derefs to reach the frame. */
	lui	a2, %hi(pxCurrentTCB)	  /* a2 = &pxCurrentTCB */
	lw	a2, %lo(pxCurrentTCB)(a2) /*    = pxCurrentTCB    */
	lw	a2, 0(a2)		  /* a2 = pxCurrentTCB->pxTopOfStack */

	/* Compute the original sp (pre-trap value) for the
	 * dispatcher.  FreeRTOS does not save x2; the misalign
	 * emulator needs it whenever a load or store uses sp as
	 * the base register. */
	li	t0, portCONTEXT_SIZE	/* t0 = frame size */
	add	a3, a2, t0		/* a3 = orig_sp = frame + size */

	/* a1 was mepc+4 on entry (the value mret will use).  Back
	 * it up to the original mepc so the C side can read the
	 * offending instruction word. */
	addi	a1, a1, -4		/* a1 = original mepc */

	/* Call: int riscv_trap_handler(mcause, mepc, frame, orig_sp);
	 * Returns 1 in a0 when handled, 0 otherwise. */
	call	riscv_trap_handler

	lw	ra, 12(sp)		/* restore return address */
	addi	sp, sp, 16		/* discard our frame */
	beqz	a0, 1f			/* unhandled -> ebreak below */
	ret				/* handled -> back to caller */

	/* *****************************************************
	 * Unhandled sync exception:
	 *   Snapshot the trap CSRs into t0/t1/t2 so they can be
	 *   inspected in the debugger, then ebreak.  Should
	 *   execution somehow continue past the ebreak, spin
	 *   forever rather than let the FreeRTOS port mret
	 *   straight back into the offending instruction.
	 * ***************************************************** */
1:	csrr	t0, mcause		/* trap cause for inspection */
	csrr	t1, mepc		/* faulting PC for inspection */
	csrr	t2, mstatus		/* saved mstatus for inspection */
	ebreak				/* halt to debugger */
2:	j	2b			/* spin if execution continues */

	.size	freertos_risc_v_application_exception_handler, . - freertos_risc_v_application_exception_handler
