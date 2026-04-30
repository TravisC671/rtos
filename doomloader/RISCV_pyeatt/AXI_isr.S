/* *********************************************************
 * Name:    AXI_isr.S
 * Author:  Larry Pyeatt
 * Date:    4/28/2026
 *
 * Asm shims for the AXI Timer 0 / Timer 1 vector entries.
 *
 * Why this exists:
 *   AXI_timer_handler() may invoke a user-registered handler
 *   (set via AXI_TIMER_set_handler) that does FromISR APIs and
 *   wants to unblock a higher-priority task.  Just as in the
 *   UART case, the simple GCC __attribute__((interrupt)) save
 *   only covers caller-saved registers, leaving sp pointing at
 *   the OLD task's stack across an mret that follows
 *   vTaskSwitchContext.  These shims do the full FreeRTOS
 *   context save/restore so handlers can safely yield.
 *
 * AXI_timer_handler is declared in AXI_timer.c as
 *   BaseType_t AXI_timer_handler(volatile AXI_timer_device_t *device);
 * and returns the HigherPriorityTaskWoken flag the user
 * handler accumulated.  The shim below passes the appropriate
 * &timer_device[i] in a0 and inspects the returned a0 to
 * decide whether to call vTaskSwitchContext before mret.
 * ********************************************************* */

#include "../FreeRTOS/portable/GCC/RISC-V/portContext.h"

	/* sizeof(AXI_timer_device_t) layout on RV32 ilp32:
	 *   TaskHandle_t owner[2]            = 2 * 4 = 8 bytes
	 *   AXI_timer_handler_t handler[2]   = 2 * 4 = 8 bytes
	 *   AXI_timer_t *device[2]           = 2 * 4 = 8 bytes
	 *   int IRQ_NUM                      = 4 bytes
	 * Total = 28 bytes.  Struct alignment is 4 (max field
	 * alignment), so no trailing padding. */
	.equ	AXI_TIMER_DEVICE_SIZE, 28

	.extern	AXI_timer_handler
	.extern	timer_device
	.extern	pxCurrentTCB
	.extern	xISRStackTop
	.extern	pxCriticalNesting
	.extern	vTaskSwitchContext

	.section .text
	.align	2

	/* ***************************************************
	 * AXI_TIMER_0_ISR:
	 *   Vector entry for platform IRQ 2 (mcause = 18).
	 *   Saves the interrupted task's full context, calls
	 *   the C handler with &timer_device[0], yields if any
	 *   user sub-handler unblocked a higher-priority task.
	 * *************************************************** */
	.global	AXI_TIMER_0_ISR
AXI_TIMER_0_ISR:
	portcontextSAVE_INTERRUPT_CONTEXT
	la	a0, timer_device		/* a0 = &timer_device[0] */
	call	AXI_timer_handler		/* a0 = HigherPriorityTaskWoken */
	beqz	a0, 1f
	call	vTaskSwitchContext
1:	portcontextRESTORE_CONTEXT
	.size	AXI_TIMER_0_ISR, . - AXI_TIMER_0_ISR

	/* ***************************************************
	 * AXI_TIMER_1_ISR:
	 *   Vector entry for platform IRQ 3 (mcause = 19).
	 *   Same pattern with &timer_device[1].
	 * *************************************************** */
	.global	AXI_TIMER_1_ISR
AXI_TIMER_1_ISR:
	portcontextSAVE_INTERRUPT_CONTEXT
	la	a0, timer_device
	addi	a0, a0, AXI_TIMER_DEVICE_SIZE	/* a0 = &timer_device[1] */
	call	AXI_timer_handler
	beqz	a0, 1f
	call	vTaskSwitchContext
1:	portcontextRESTORE_CONTEXT
	.size	AXI_TIMER_1_ISR, . - AXI_TIMER_1_ISR
