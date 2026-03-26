//------------------------------------------------------------------------------
// Copyright (c) 2026 Larry D. Pyeatt
// All rights reserved.
//------------------------------------------------------------------------------
// VGA Controller Driver - FreeRTOS Header (320x200 Double-Buffered Mode)
//
// Hardware double-buffering strategy
// -----------------------------------
// The VGA controller has two framebuffer regions within its 256 KB address
// space and a Display Buffer Register (DBR) that selects which one the scan-out
// reads.  In 320x200 mode each frame is 320 x 200 = 64,000 bytes, well within
// the 128 KB half of the framebuffer, so both buffers sit in hardware.
//
// Naming convention used throughout:
//   hw_buf[0]  VGA hardware buffer 0 -- scanned when DBR = 0
//   hw_buf[1]  VGA hardware buffer 1 -- scanned when DBR = 1
//
// The CPU always draws into the buffer that is NOT currently displayed
// (the "back" buffer).  At each VSYNC:
//   1. The ISR flips the Display Buffer Register so the just-rendered
//      buffer becomes the front buffer.
//   2. The ISR sends the new back-buffer index to the render task via
//      a single-item queue.
//   3. The render task draws the next frame into the new back buffer
//      and then waits for the next VSYNC flip before touching the
//      hardware again.
//
// This approach provides tear-free animation with zero memcpy overhead.
//
// Static allocation policy
// -------------------------
// All FreeRTOS objects (queue, task TCB and stack, idle task, timer task)
// are allocated statically in .bss -- no heap calls are made by application
// code.  The linker script places .bss and .data in fast RAM, so all
// FreeRTOS objects automatically reside in fast RAM with no section
// attributes required.
//------------------------------------------------------------------------------

#ifndef VGA_DRIVER_H
#define VGA_DRIVER_H

#include <stdint.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <queue.h>
#include <task.h>

//------------------------------------------------------------------------------
// Hardware Register Definitions
//------------------------------------------------------------------------------

#define VGA_FRAMEBUFFER_BASE    0x48000000
#define VGA_PALETTE_BASE        0x48040000
#define VGA_CONTROL_REG         0x48040200
#define VGA_DISPLAY_BUF_REG     0x48040204
#define VGA_IRQ_ENABLE_REG      0x48040208  // VSIEN in bit 0 (R/W)
#define VGA_IRQ_CLEAR_REG       0x4804020C  // VSIA  in bit 0 (R/W1C)

// 320x200 framebuffer geometry
#define FRAMEBUFFER_WIDTH       320
#define FRAMEBUFFER_HEIGHT      200
#define FRAMEBUFFER_SIZE        (FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT)  // 64000 bytes

// The 256 KB VGA framebuffer holds two 320x200 buffers packed end-to-end.
// Buffer 1 starts immediately after buffer 0: 320*200 = 64,000 bytes = 0xFA00.
// This matches FB_BUFFER_1_BASE = 64000 in vga_timing_pkg.vhdl.
// Each half comfortably holds one 320x200 frame (64,000 bytes).
#define VGA_FRAMEBUFFER_BUF0    VGA_FRAMEBUFFER_BASE
#define VGA_FRAMEBUFFER_BUF1    (VGA_FRAMEBUFFER_BASE + 0x0FA00)  // +64,000 bytes

// Control register bits
#define VGA_CTRL_MODE_320x200   (1 << 0)  // 1 = 320x200 double-buffer mode

// Display Buffer Register values
#define VGA_DISP_BUFFER_0       (0 << 0)  // scan out buffer 0
#define VGA_DISP_BUFFER_1       (1 << 0)  // scan out buffer 1

// IRQ Enable Register bits
#define VGA_IRQ_VSIEN           (1 << 0)  // vertical sync interrupt enable

// IRQ Clear/Status Register bits
#define VGA_IRQ_VSIA            (1 << 0)  // W1C: write 1 to clear pending flag
                                          // Read: 1 = interrupt pending

//------------------------------------------------------------------------------
// Stack depth for the render task (words, not bytes)
//------------------------------------------------------------------------------

#define RENDER_TASK_STACK_WORDS   512

//------------------------------------------------------------------------------
// Inter-task Communication
//
// back_buf_queue -- statically allocated length-1 queue carrying the index
//                   (0 or 1) of the hardware buffer the render task should
//                   draw into next.  Written by the ISR; read by render_task.
//------------------------------------------------------------------------------

extern QueueHandle_t back_buf_queue;
extern QueueHandle_t frame_done_queue;  // render task posts here when a frame is complete

//------------------------------------------------------------------------------
// Function Prototypes
//------------------------------------------------------------------------------

void vga_init(void);
void vga_isr(void);
void render_task(void *pvParameters);

#endif // VGA_DRIVER_H
