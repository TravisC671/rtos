//------------------------------------------------------------------------------
// Copyright (c) 2026 Larry D. Pyeatt
// All rights reserved.
//------------------------------------------------------------------------------
// VGA Controller Driver - FreeRTOS Header (640x400 Mode)
//------------------------------------------------------------------------------

#ifndef VGA_DRIVER_H
#define VGA_DRIVER_H

#include <stdint.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

//------------------------------------------------------------------------------
// Hardware Register Definitions
//------------------------------------------------------------------------------

// Base addresses
#define VGA_FRAMEBUFFER_BASE    0x48000000
#define VGA_PALETTE_BASE        0x48040000
#define VGA_CONTROL_REG         0x48040200
#define VGA_DISPLAY_BUF_REG     0x48040204
#define VGA_IRQ_ENABLE_REG      0x48040208  // VSIEN in bit 0 (R/W)
#define VGA_IRQ_CLEAR_REG       0x4804020C  // VSIA  in bit 0 (R/W1C)

// Framebuffer size for 640x400 mode
#define FRAMEBUFFER_WIDTH       640
#define FRAMEBUFFER_HEIGHT      400
#define FRAMEBUFFER_SIZE        (FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT)  // 256000 bytes

// Control register bits
#define VGA_CTRL_MODE_640x400   (0 << 0)  // 0 = 640x400 mode

// Display buffer register bits (not used in 640x400 - no double buffering)
#define VGA_DISP_BUFFER_0       (0 << 0)

// IRQ Enable Register bits
#define VGA_IRQ_VSIEN           (1 << 0)  // vertical sync interrupt enable

// IRQ Clear/Status Register bits
#define VGA_IRQ_VSIA            (1 << 0)  // W1C: write 1 to clear pending flag
                                          // Read: 1 = interrupt pending

//------------------------------------------------------------------------------
// Global Semaphores
//------------------------------------------------------------------------------

extern SemaphoreHandle_t vsync_semaphore;
extern SemaphoreHandle_t draw_frame_semaphore;

//------------------------------------------------------------------------------
// Shared Video Buffer
//------------------------------------------------------------------------------

extern uint8_t *video_buffer;

//------------------------------------------------------------------------------
// Assembly fast-fill and fast-copy -- see include/asm_string.h
//
// vga_clear_buffer_asm() and vga_copy_buffer_asm() are declared in
// asm_string.h (implemented in src/memset_fast.S and src/memcpy_fast.S).
// Include asm_string.h directly when those functions are needed.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Function Prototypes
//------------------------------------------------------------------------------

// Initialize VGA hardware and create semaphores
void vga_init(void);

// VGA interrupt service routine
void vga_isr(void);

// FreeRTOS tasks
void vga_update_task(void *pvParameters);
void render_task(void *pvParameters);

#endif // VGA_DRIVER_H
