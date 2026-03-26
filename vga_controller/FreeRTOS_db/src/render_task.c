//------------------------------------------------------------------------------
// Copyright (c) 2026 Larry D. Pyeatt
// All rights reserved.
//------------------------------------------------------------------------------
// Render Task - 320x200 Double-Buffered Mode
//
// Draws the next animation frame directly into the hardware back buffer
// (the half of the VGA framebuffer that is NOT currently being scanned out).
// No intermediate RAM buffer and no memcpy are needed.
//
// Execution flow
// --------------
//   1. Block on back_buf_queue until the ISR delivers the index of the
//      buffer to draw into.  This happens once per VSYNC (~14.3 ms).
//   2. Compute a pointer to the correct hardware buffer half.
//   3. Render the frame in-place.
//   4. Loop — the frame will be flipped to front on the next VSYNC.
//
// If rendering takes longer than one VSYNC period the ISR sees an empty
// frame_done_queue and holds the current front buffer for another VSYNC.
// The render task naturally drops frames rather than accumulating latency.
//------------------------------------------------------------------------------

#include "vga_driver.h"
#include "asm_string.h"

//------------------------------------------------------------------------------
// Graphics primitives
// All operate on a caller-supplied buffer pointer so they work regardless
// of which hardware buffer is currently the back buffer.
//------------------------------------------------------------------------------

static void clear_buffer(uint8_t *buf, uint8_t color)
{
  vga_clear_buffer_asm(buf, color, FRAMEBUFFER_SIZE);
}

static void set_pixel(uint8_t *buf, uint16_t x, uint16_t y, uint8_t color)
{
  if (x < FRAMEBUFFER_WIDTH && y < FRAMEBUFFER_HEIGHT)
    buf[y * FRAMEBUFFER_WIDTH + x] = color;
}

static void draw_rect(uint8_t *buf,
                      uint16_t x, uint16_t y,
                      uint16_t w, uint16_t h, uint8_t color)
{
  for (uint16_t dy = 0; dy < h; dy++)
    for (uint16_t dx = 0; dx < w; dx++)
      set_pixel(buf, x + dx, y + dy, color);
}

static void draw_hline(uint8_t *buf,
                       uint16_t x, uint16_t y, uint16_t length, uint8_t color)
{
  for (uint16_t dx = 0; dx < length; dx++)
    set_pixel(buf, x + dx, y, color);
}

//------------------------------------------------------------------------------
// render_task
//------------------------------------------------------------------------------

void render_task(void *pvParameters)
{
  (void)pvParameters;

  // Pointers to the two hardware buffer halves.
  uint8_t * const hw_buf[2] = {
    (uint8_t *)VGA_FRAMEBUFFER_BUF0,
    (uint8_t *)VGA_FRAMEBUFFER_BUF1
  };

  // Animation state.  int16_t avoids unsigned underflow when dx/dy are
  // negative and the position reaches the left or top edge.
  int16_t box_x = 0;
  int16_t box_y = 0;
  int16_t dx    = 2;
  int16_t dy    = 1;

  // Box dimensions — scaled for 320x200
  const int16_t BOX_W = 32;
  const int16_t BOX_H = 32;

  for (;;)
    {
      // Wait for the ISR to tell us which buffer to draw into.
      uint8_t back_idx;
      xQueueReceive(back_buf_queue, &back_idx, portMAX_DELAY);

      uint8_t *back = hw_buf[back_idx];

      // --- Render frame into back buffer ---

      // Clear to background (palette index 0 = black)
      clear_buffer(back, 0);

      // Draw the bouncing box (palette index 31 = bright red)
      draw_rect(back, box_x, box_y, BOX_W, BOX_H, 31);

      // Draw a white border around the screen (palette index 15 = white).
      // The vertical lines are written as direct byte assignments to
      // guarantee they land on column 0 and column FRAMEBUFFER_WIDTH-1
      // with no arithmetic that could produce an off-by-one.
      draw_hline(back, 0, 0,                    FRAMEBUFFER_WIDTH, 15);
      draw_hline(back, 0, FRAMEBUFFER_HEIGHT-1, FRAMEBUFFER_WIDTH, 15);
      for (uint16_t row = 0; row < FRAMEBUFFER_HEIGHT; row++)
        {
          back[row * FRAMEBUFFER_WIDTH]                        = 15;
          back[row * FRAMEBUFFER_WIDTH + (FRAMEBUFFER_WIDTH-1)] = 15;
        }

      // Signal to the ISR that the back buffer is fully rendered and safe
      // to flip.  This must happen after the last pixel write and before
      // the animation state update (which does not touch the buffer).
      // xQueueOverwrite is used so a late render that misses a VSYNC simply
      // replaces the previous token rather than blocking.
      uint8_t done_token = 1;
      xQueueOverwrite(frame_done_queue, &done_token);

      // --- Update animation state for next frame ---

      box_x += dx;
      box_y += dy;

      // Bounce off edges, clamping position so the box never goes out of bounds.
      if (box_x <= 0)
        { box_x = 0; dx = -dx; }
      else if (box_x >= (int16_t)(FRAMEBUFFER_WIDTH - BOX_W))
        { box_x = (int16_t)(FRAMEBUFFER_WIDTH - BOX_W); dx = -dx; }

      if (box_y <= 0)
        { box_y = 0; dy = -dy; }
      else if (box_y >= (int16_t)(FRAMEBUFFER_HEIGHT - BOX_H))
        { box_y = (int16_t)(FRAMEBUFFER_HEIGHT - BOX_H); dy = -dy; }
    }
}
