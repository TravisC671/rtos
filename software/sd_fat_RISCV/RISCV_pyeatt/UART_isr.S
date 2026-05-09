/* *********************************************************
 * Name:    UART_isr.S
 * Author:  Larry Pyeatt
 * Date:    4/28/2026
 *
 * Asm shims for UART0 / UART1 vector entries.
 *
 * Why this exists:
 *   The C-level UART handler (UART_handler in
 *   PDCurses/ANSI/drivers/FreeRTOS/UART_16550.c) calls
 *   xStreamBufferSendFromISR / xStreamBufferReceiveFromISR,
 *   which can unblock a higher-priority task waiting on the
 *   stream buffer.  When that happens portYIELD_FROM_ISR
 *   eventually calls vTaskSwitchContext, which updates
 *   pxCurrentTCB.  After that, the trap epilogue must reload
 *   sp from the new pxCurrentTCB->pxTopOfStack and restore
 *   that task's full register frame.
 *
 *   The previous C handlers used __attribute__((interrupt)),
 *   which on RISC-V only saves the caller-saved registers to
 *   the *interrupted* task's sp and mret's back to that same
 *   sp.  After a context switch pxCurrentTCB has changed but
 *   sp has not, so execution continues on the OLD task's stack
 *   with the NEW TCB pointer, immediately corrupting both
 *   tasks.  Symptom: firework wedges in initscr after the
 *   cursor-position reply unblocks it from the UART0 RX
 *   stream buffer.
 *
 *   These shims do the same full-context save/restore that
 *   the FreeRTOS-provided mtimer handler uses, with the same
 *   `if (HigherPriorityTaskWoken) vTaskSwitchContext;` test.
 *
 * The C side declares UART_handler as non-static and returns
 * the HigherPriorityTaskWoken value in a0; the previous C
 * UART0_handler / UART1_handler with __attribute__((interrupt))
 * must be removed so the linker uses these globals.
 * ********************************************************* */

#include "../FreeRTOS/portable/GCC/RISC-V/portContext.h"

	/* sizeof(UART_16550_descriptor_t) on RV32 ilp32: seven 4-byte
	 * fields (dev, interrupt_number, RX_buffer, TX_buffer, RX_mutex,
	 * TX_mutex, tx_state), no padding = 28 bytes. */
	.equ	UART_DESCRIPTOR_SIZE, 28

	.extern	UART_handler
	.extern	uart
	.extern	pxCurrentTCB
	.extern	xISRStackTop
	.extern	pxCriticalNesting
	.extern	vTaskSwitchContext

	.section .text
	.align	2

	/* ***************************************************
	 * UART0_handler:
	 *   Vector entry for platform IRQ 0 (mcause = 16).
	 *   Saves the interrupted task's full context to its
	 *   own stack, switches to the ISR stack, calls the C
	 *   driver with a pointer to uart[0], and on return
	 *   yields to a higher-priority woken task if any.
	 * *************************************************** */
	.global	UART0_handler
UART0_handler:
	portcontextSAVE_INTERRUPT_CONTEXT
	la	a0, uart		/* a0 = &uart[0]                  */
	call	UART_handler		/* a0 = HigherPriorityTaskWoken    */
	beqz	a0, 1f
	call	vTaskSwitchContext
1:	portcontextRESTORE_CONTEXT
	.size	UART0_handler, . - UART0_handler
