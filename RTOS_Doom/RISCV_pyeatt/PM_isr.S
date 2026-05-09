/* *********************************************************
 * Name:    PM_isr.S
 * Author:  Larry Pyeatt
 * Date:    4/28/2026
 *
 * Asm shim for the LDP-001 pulse modulator (PM) vector entry.
 *
 * Same rationale as UART_isr.S and AXI_isr.S: PM_handler_body
 * may invoke a user-registered channel handler that calls
 * FromISR APIs and wants to unblock a higher-priority task.
 * The simple GCC __attribute__((interrupt)) save covers only
 * caller-saved registers, leaving sp pointing at the OLD
 * task's stack across mret following vTaskSwitchContext.
 * This shim does the full FreeRTOS context save/restore.
 *
 * PM_handler_body is declared in LDP-001_PM_driver.c as
 *   BaseType_t PM_handler_body(void);
 * and returns the HigherPriorityTaskWoken flag the channel
 * sub-handlers accumulated.  The shim inspects the returned
 * a0 to decide whether to call vTaskSwitchContext before mret.
 * ********************************************************* */

#include "../FreeRTOS/portable/GCC/RISC-V/portContext.h"

	.extern	PM_handler_body
	.extern	pxCurrentTCB
	.extern	xISRStackTop
	.extern	pxCriticalNesting
	.extern	vTaskSwitchContext

	.section .text
	.align	2

	/* ***************************************************
	 * PM_handler:
	 *   Vector entry for platform IRQ 7 (mcause = 23).
	 *   Saves the interrupted task's full context, calls
	 *   PM_handler_body which iterates the channel
	 *   descriptors, yields if any sub-handler unblocked a
	 *   higher-priority task.
	 * *************************************************** */
	.global	PM_handler
PM_handler:
	portcontextSAVE_INTERRUPT_CONTEXT
	call	PM_handler_body			/* a0 = HigherPriorityTaskWoken */
	beqz	a0, 1f
	call	vTaskSwitchContext
1:	portcontextRESTORE_CONTEXT
	.size	PM_handler, . - PM_handler
