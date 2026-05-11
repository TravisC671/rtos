#include "../FreeRTOS/portable/GCC/RISC-V/portContext.h"

	.extern	VGA_handler_body
	.extern	pxCurrentTCB
	.extern	xISRStackTop
	.extern	pxCriticalNesting
	.extern	vTaskSwitchContext

	.section .text
	.align	2

	/* ***************************************************
	 * VGA_handler:
	 *   Vector entry for platform IRQ 8.
	 *   Saves the interrupted task's full context, calls
	 *   VGA_handler_body which iterates the channel
	 *   descriptors, yields if any sub-handler unblocked a
	 *   higher-priority task.
	 * *************************************************** */
	.global	VGA_handler
VGA_handler:
	portcontextSAVE_INTERRUPT_CONTEXT
	call	VGA_handler_body			/* a0 = HigherPriorityTaskWoken */
	beqz	a0, 1f
	call	vTaskSwitchContext
1:	portcontextRESTORE_CONTEXT
	.size	VGA_handler, . - VGA_handler
