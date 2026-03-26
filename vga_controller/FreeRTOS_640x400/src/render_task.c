//------------------------------------------------------------------------------
// Copyright (c) 2026 Larry D. Pyeatt
// All rights reserved.
//------------------------------------------------------------------------------
// Render Task - Draws the next frame into shared video buffer
//------------------------------------------------------------------------------

#include "vga_driver.h"
#include "asm_string.h"

//------------------------------------------------------------------------------
// Example: Simple Graphics Helper Functions
//------------------------------------------------------------------------------

// Clear the video buffer to a single color using STMIA burst writes.
// vga_clear_buffer_asm() stores 32 bytes per iteration via STMIA, which
// the AXI interconnect issues as an 8-word burst.  video_buffer is heap-
// allocated (at least 4-byte aligned) and FRAMEBUFFER_SIZE (256000) is
// divisible by 32, so no head/tail handling is required.
static void clear_buffer(uint8_t color)
{
  vga_clear_buffer_asm(video_buffer, color, FRAMEBUFFER_SIZE);
}

// Set a pixel at (x, y) to a given color (640x400 mode)
static void set_pixel(uint16_t x, uint16_t y, uint8_t color)
{
  if (x < FRAMEBUFFER_WIDTH && y < FRAMEBUFFER_HEIGHT)
    video_buffer[y * FRAMEBUFFER_WIDTH + x] = color;
}

// Draw a filled rectangle
static void draw_rect(uint16_t x, uint16_t y,
		      uint16_t w, uint16_t h, uint8_t color)
{
  for (uint16_t dy = 0; dy < h; dy++)
    for (uint16_t dx = 0; dx < w; dx++)
      set_pixel(x + dx, y + dy, color);
}

// Draw a horizontal line
static void draw_hline(uint16_t x, uint16_t y, uint16_t length, uint8_t color)
{
  for (uint16_t dx = 0; dx < length; dx++)
    set_pixel(x + dx, y, color);
}

// Draw a vertical line
static void draw_vline(uint16_t x, uint16_t y, uint16_t length, uint8_t color)
{
  for (uint16_t dy = 0; dy < length; dy++)
    set_pixel(x, y + dy, color);
}

//------------------------------------------------------------------------------
// Render Task
//------------------------------------------------------------------------------
// Waits for permission to draw (via semaphore), then renders the next frame
// into the shared video buffer in RAM

void render_task(void *pvParameters)
{
  (void)pvParameters;  // Unused parameter
    
  uint32_t frame_count = 0;
  int16_t box_x = 0;
  int16_t box_y = 0;
  int16_t dx = 3;
  int16_t dy = 2;
    
  for (;;)
    {
      // Wait for permission to draw (released by VGA update task after copy)
      if (xSemaphoreTake(draw_frame_semaphore, portMAX_DELAY) == pdTRUE)
        {
	  // --- Example animation: Bouncing box with frame counter ---
            
	  // Clear screen to background color (palette index 0)
	  clear_buffer(0);
            
	  // Draw a moving filled box (palette index 31)
	  draw_rect(box_x, box_y, 64, 64, 31);
            
	  // Draw a border around the screen (palette index 15)
	  draw_hline(0, 0, FRAMEBUFFER_WIDTH, 15);
	  draw_hline(0, FRAMEBUFFER_HEIGHT - 1, FRAMEBUFFER_WIDTH, 15);
	  draw_vline(0, 0, FRAMEBUFFER_HEIGHT, 15);
	  draw_vline(FRAMEBUFFER_WIDTH - 1, 0, FRAMEBUFFER_HEIGHT, 15);
            
	  // Update position for next frame
	  box_x += dx;
	  box_y += dy;
            
	  // Bounce off edges, clamping so the box never goes out of bounds.
	  if (box_x <= 0)
	    { box_x = 0; dx = -dx; }
	  else if (box_x >= (int16_t)(FRAMEBUFFER_WIDTH - 64))
	    { box_x = (int16_t)(FRAMEBUFFER_WIDTH - 64); dx = -dx; }

	  if (box_y <= 0)
	    { box_y = 0; dy = -dy; }
	  else if (box_y >= (int16_t)(FRAMEBUFFER_HEIGHT - 64))
	    { box_y = (int16_t)(FRAMEBUFFER_HEIGHT - 64); dy = -dy; }
            
	  frame_count++;
            
	  // --- End of example animation ---
            
	  // NOTE: Replace the above example with your actual rendering code
	  // This is where you would:
	  // - Draw game graphics at 640x400 resolution
	  // - Render UI elements
	  // - Update sprite positions
	  // - Copy pre-rendered tiles
	  // - Draw text using bitmap fonts
	  // - Implement double-buffering effects
	  // - etc.

	  // Signal the VGA update task that this frame is ready to copy.
	  // This MUST be called at the end of every rendering pass so that
	  // vga_update_task's non-blocking xSemaphoreTake succeeds at the
	  // next VSYNC and copies video_buffer to the hardware framebuffer.
	  // Without this give, vga_update_task never copies and the screen
	  // stays black.
	  xSemaphoreGive(draw_frame_semaphore);
        }
    }
}
