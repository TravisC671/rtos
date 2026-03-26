//------------------------------------------------------------------------------
// Copyright (c) 2026 Larry D. Pyeatt
// All rights reserved.
//------------------------------------------------------------------------------
// VGA Interrupt Service Routine - 320x200 Double-Buffered Mode
//
// Runs at each VSYNC (~70 Hz).  Responsibilities:
//   1. Clear the VGA interrupt (W1C) and re-enable it.
//   2. Check whether the render task has finished the current frame.
//   3. If finished: flip the Display Buffer Register and assign the new
//      back buffer to the render task via back_buf_queue.
//      If not finished: leave the DBR unchanged and skip this frame so
//      the display continues to show the last completed frame.
//
// Render-task completion check
// ----------------------------
// The render task signals completion explicitly by posting a dummy byte to
// frame_done_queue immediately after writing the last pixel of a frame.
// The ISR checks frame_done_queue at each VSYNC:
//
//   frame_done_queue non-empty => render task has finished drawing and the
//                                 back buffer is safe to promote to front.
//
//   frame_done_queue empty     => render task is still drawing (or has not
//                                 yet been scheduled); skip this flip and
//                                 continue displaying the current front buffer.
//
// Why not use back_buf_queue emptiness as the signal?
// ---------------------------------------------------
// xQueueReceive() removes the item from back_buf_queue the instant the render
// task wakes, before a single pixel has been written.  So an empty
// back_buf_queue only means the task started, not that it finished.
//
// The flip is atomic from the display's perspective: the VGA controller
// latches the DBR value at the start of each frame, so writing it during
// vertical blanking is always tear-free.
//
// No memcpy is ever performed.  The render task writes directly into the
// hardware framebuffer that is not currently being scanned out.
//------------------------------------------------------------------------------

#include <device_addrs.h>
#include <vga_driver.h>
#include <core_cm3.h>

//------------------------------------------------------------------------------
// Module-private state
//
// current_front is the index (0 or 1) of the buffer currently displayed.
// It is only updated when a flip actually occurs, keeping it in sync with
// the Display Buffer Register at all times.
//------------------------------------------------------------------------------

static volatile uint8_t current_front = 0;  // index of buffer being scanned out

//------------------------------------------------------------------------------
// vga_isr
//------------------------------------------------------------------------------

void vga_isr(void)
{
  BaseType_t higher_priority_task_woken = pdFALSE;
  volatile uint32_t *irq_clear_reg   = (volatile uint32_t *)VGA_IRQ_CLEAR_REG;
  volatile uint32_t *display_buf_reg = (volatile uint32_t *)VGA_DISPLAY_BUF_REG;

  // Clear the interrupt: write 1 to bit 0 of the IRQ Clear/Status register.
  *irq_clear_reg = VGA_IRQ_VSIA;

  // Clear the NVIC pending bit.
  NVIC_ClearPendingIRQ((IRQn_Type)VGA_IRQ);

  // Check whether the render task has finished drawing the back buffer.
  // The render task posts a dummy byte to frame_done_queue after writing
  // the very last pixel, so a non-empty queue is an unambiguous "done" signal.
  uint8_t done_token;
  if (xQueueReceiveFromISR(frame_done_queue, &done_token,
                           &higher_priority_task_woken) == pdTRUE)
    {
      // Render is complete -- flip the display to show the finished frame.
      current_front ^= 1;
      *display_buf_reg = (current_front == 0) ? VGA_DISP_BUFFER_0
                                              : VGA_DISP_BUFFER_1;

      // Tell the render task which buffer is now free to draw into
      // (the one just retired from display).
      uint8_t back = current_front ^ 1;
      xQueueSendFromISR(back_buf_queue, &back, &higher_priority_task_woken);
    }
  // else: render task is still drawing; hold the current front buffer for
  // another frame.  The next VSYNC will check frame_done_queue again.

  portYIELD_FROM_ISR(higher_priority_task_woken);
}
