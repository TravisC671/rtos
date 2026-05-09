/* *********************************************************
 * Name:    SD_isr.S
 * Author:  Larry Pyeatt
 * Date:    4/28/2026
 *
 * Asm shim for the SD Card (SD) vector entry.
 *
 * Same rationale as UART_isr.S and AXI_isr.S: SD_handler
 * may invoke a user-registered channel handler that calls
 * FromISR APIs and wants to unblock a higher-priority task.
 * The simple GCC __attribute__((interrupt)) save covers only
 * caller-saved registers, leaving sp pointing at the OLD
 * task's stack across mret following vTaskSwitchContext.
 * This shim does the full FreeRTOS context save/restore.
 * ********************************************************* */

#include "../FreeRTOS/portable/GCC/RISC-V/portContext.h"

	.extern	SD_handler_body
	.extern	pxCurrentTCB
	.extern	xISRStackTop
	.extern	pxCriticalNesting
	.extern	vTaskSwitchContext

	.section .text
	.align	2

	/* ***************************************************
	 * SD_handler:
	 *   Vector entry for platform IRQ 8.
	 *   Saves the interrupted task's full context, calls
	 *   SD_handler_body which iterates the channel
	 *   descriptors, yields if any sub-handler unblocked a
	 *   higher-priority task.
	 * *************************************************** */
	.global	SD_handler
SD_handler:
	portcontextSAVE_INTERRUPT_CONTEXT
	call	SD_handler_body			/* a0 = HigherPriorityTaskWoken */
	beqz	a0, 1f
	call	vTaskSwitchContext
1:	portcontextRESTORE_CONTEXT
	.size	SD_handler, . - SD_handler
