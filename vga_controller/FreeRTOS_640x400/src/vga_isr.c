//------------------------------------------------------------------------------
// Copyright (c) 2026 Larry D. Pyeatt
// All rights reserved.
//------------------------------------------------------------------------------
// VGA Interrupt Service Routine - Cortex-M3
//------------------------------------------------------------------------------
#include <device_addrs.h>
#include <vga_driver.h>
#include <core_cm3.h>

//------------------------------------------------------------------------------
// VGA Interrupt Service Routine
//------------------------------------------------------------------------------
// Called when VSYNC interrupt fires (every 14.3ms @ 70Hz).
// Acknowledges the interrupt and signals the VGA update task via semaphore.
//
// The interrupt hardware uses two separate registers:
//   VGA_IRQ_ENABLE_REG (0x48040208) -- bit 0 = VSIEN, written once at init
//   VGA_IRQ_CLEAR_REG  (0x4804020C) -- bit 0 = VSIA,  W1C: write 1 to clear
//
// Writing to VGA_IRQ_CLEAR_REG does NOT disturb VSIEN; the enable register
// is left untouched here.

void vga_isr(void)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  volatile uint32_t *irq_clear_reg = (volatile uint32_t *)VGA_IRQ_CLEAR_REG;

  // Acknowledge: write 1 to bit 0 of the IRQ Clear/Status register (W1C).
  *irq_clear_reg = VGA_IRQ_VSIA;

  // Clear the NVIC pending bit for this IRQ.
  NVIC_ClearPendingIRQ((IRQn_Type)VGA_IRQ);

  // Release the vsync semaphore to wake up the VGA update task.
  xSemaphoreGiveFromISR(vsync_semaphore, &xHigherPriorityTaskWoken);

  // Perform context switch if a higher priority task was woken.
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
