//------------------------------------------------------------------------------
// Copyright (c) 2026 Larry D. Pyeatt
// All rights reserved.
//------------------------------------------------------------------------------
// VGA Update Task - Copies video buffer to VGA framebuffer
//------------------------------------------------------------------------------

#include "vga_driver.h"
#include "asm_string.h"

//------------------------------------------------------------------------------
// VGA Update Task
//------------------------------------------------------------------------------
// Waits for VSYNC (via semaphore), then copies the shared video buffer
// to the VGA framebuffer hardware, then signals the render task.
//
// FRAME SKIP PROTECTION:
// Only copies if the render task has completed the previous frame.
// If render is still working, this frame is not copied.
// This prevents tearing from copying a partially-rendered frame.
//
// TIMING ANALYSIS (640x400 @ 70 Hz, 50 MHz AXI, DDR2 RAM):
//   Vertical blanking period: ~1.56 ms (49 lines)
//   Copy time with DDR2:      ~0.80 ms (80% bus efficiency)
//   Safety margin:            1.95x   (uses ~51% of V-blank time)
//
// The copy uses vga_copy_buffer_asm() from memcpy_fast.S.  Both pointers
// are word-aligned (VGA_FRAMEBUFFER_BASE is fixed hardware; video_buffer
// comes from pvPortMalloc which aligns to at least 4 bytes) and
// FRAMEBUFFER_SIZE (256,000) is a multiple of 32, so the function goes
// straight into the paired LDMIA+STMIA burst loop for 8,000 iterations.
// Each iteration issues eight AXI read transactions followed by eight AXI
// write transactions, keeping both bus directions busy back-to-back.

void vga_update_task(void *pvParameters)
{
  (void)pvParameters;

  volatile uint8_t *framebuffer = (volatile uint8_t *)VGA_FRAMEBUFFER_BASE;

  for (;;)
    {
      // Wait for VSYNC interrupt (blocks until ISR releases semaphore).
      if (xSemaphoreTake(vsync_semaphore, portMAX_DELAY) == pdTRUE)
        {
          // Check if render task has finished the frame (non-blocking).
          if (xSemaphoreTake(draw_frame_semaphore, 0) == pdTRUE)
            {
              // Render task has finished -- copy 256,000 bytes from the
              // software framebuffer in RAM to the VGA hardware framebuffer.
              // vga_copy_buffer_asm() uses LDMIA+STMIA burst pairs:
              //   8,000 iterations x 32 bytes = 256,000 bytes total.
              vga_copy_buffer_asm((void *)framebuffer,
                                  video_buffer,
                                  FRAMEBUFFER_SIZE);

              // Signal the render task that it can draw the next frame.
              xSemaphoreGive(draw_frame_semaphore);
            }
          else
            {
              // Render task has not finished -- skip this VSYNC.
              // The previous frame remains displayed (no tearing).
            }
        }
    }
}
