//------------------------------------------------------------------------------
// Copyright (c) 2026 Larry D. Pyeatt
// All rights reserved.
//------------------------------------------------------------------------------
// Main Application - VGA Controller with FreeRTOS (320x200 Double-Buffered)
//
// Architecture overview
// ---------------------
// This version exploits the VGA controller's hardware double-buffering
// support available in 320x200 mode.  The 256 KB framebuffer is split into
// two 128 KB halves; the Display Buffer Register (DBR) selects which half
// the scan-out logic reads each frame.
//
//   Hardware buffers (in VGA address space, never copied):
//     buf[0]  0x48000000 .. 0x4801F3FF   (320x200 = 64000 bytes)
//     buf[1]  0x48020000 .. 0x4803F3FF
//
//   Tasks:
//     render_task  -- draws the next frame directly into the back buffer;
//                     blocked on back_buf_queue between frames.
//
//   ISR (vga_isr):
//     Fires at VSYNC (~70 Hz).  Flips the DBR, then sends the new back
//     buffer index to back_buf_queue via xQueueOverwriteFromISR so the
//     render task wakes and starts drawing.
//
// Static allocation policy
// -------------------------
// Every FreeRTOS object is allocated statically so the heap is never
// touched by application code.  All TCBs, stacks, and the queue storage
// block are plain C variables at file scope, placing them in .bss.
// The linker script places .bss and .data in fast RAM, so all FreeRTOS
// objects automatically benefit from fast RAM with no section attributes
// or other annotations required.
//
//   Object                   Type                  Size (bytes, CM3/32-bit)
//   ----------------------   -------------------   ------------------------
//   back_buf_queue storage   StaticQueue_t              ~80
//   back_buf queue item      uint8_t[1]                  1
//   frame_done_queue storage StaticQueue_t              ~80
//   frame_done queue item    uint8_t[1]                  1
//   render task TCB          StaticTask_t              ~100
//   render task stack        StackType_t[512]          2048
//   idle task TCB            StaticTask_t              ~100
//   idle task stack          StackType_t[256]          1024
//   timer task TCB           StaticTask_t              ~100
//   timer task stack         StackType_t[1024]         4096
//   ----------------------   -------------------   ------------------------
//   Total in fast RAM                               ~7549 bytes (~7.4 KB)
//------------------------------------------------------------------------------

#include <device_addrs.h>
#include <vga_driver.h>
#include <core_cm3.h>
#include "asm_string.h"

//------------------------------------------------------------------------------
// Queue storage -- statically allocated, lands in .bss -> fast RAM.
//
// xQueueCreateStatic() requires:
//   - A buffer for the queue items: uxQueueLength * uxItemSize bytes.
//   - A StaticQueue_t control block (opaque FreeRTOS structure).
// Both are declared here at file scope so they land in .bss, which the
// linker script places in fast RAM.  No section attributes are needed.
//------------------------------------------------------------------------------

static StaticQueue_t  back_buf_queue_cb;              // control block
static uint8_t        back_buf_queue_storage[1];      // 1 item x sizeof(uint8_t)
QueueHandle_t         back_buf_queue = NULL;

static StaticQueue_t  frame_done_queue_cb;            // control block
static uint8_t        frame_done_queue_storage[1];    // 1 item x sizeof(uint8_t)
QueueHandle_t         frame_done_queue = NULL;

//------------------------------------------------------------------------------
// Render task static storage
//------------------------------------------------------------------------------

static StaticTask_t   render_task_tcb;
static StackType_t    render_task_stack[RENDER_TASK_STACK_WORDS];

//------------------------------------------------------------------------------
// Idle task static storage (required by configSUPPORT_STATIC_ALLOCATION = 1)
//------------------------------------------------------------------------------

static StaticTask_t   idle_task_tcb;
static StackType_t    idle_task_stack[configMINIMAL_STACK_SIZE];

//------------------------------------------------------------------------------
// Timer task static storage (required by configUSE_TIMERS = 1)
//------------------------------------------------------------------------------

static StaticTask_t   timer_task_tcb;
static StackType_t    timer_task_stack[configTIMER_TASK_STACK_DEPTH];

//------------------------------------------------------------------------------
// vga_init
//------------------------------------------------------------------------------

void vga_init(void)
{
  volatile uint32_t *control_reg    = (volatile uint32_t *)VGA_CONTROL_REG;
  volatile uint32_t *display_reg    = (volatile uint32_t *)VGA_DISPLAY_BUF_REG;
  volatile uint32_t *irq_enable_reg = (volatile uint32_t *)VGA_IRQ_ENABLE_REG;
  volatile uint16_t *palette        = (volatile uint16_t *)VGA_PALETTE_BASE;

  // Set 320x200 double-buffer mode (MODE bit = 1).
  *control_reg = VGA_CTRL_MODE_320x200;

  // Start with buffer 0 as the front (displayed) buffer.
  // The queue is pre-loaded with index 1 below, so the render task draws
  // its first frame into buffer 1 while buffer 0 (black) is displayed.
  // The first VSYNC ISR will flip the DBR to show buffer 1.
  *display_reg = VGA_DISP_BUFFER_0;

  // Initialize palette -- same layout as the 640x400 version so the
  // bouncing-box colours (indices 0, 15, 31) are identical.
  for (uint16_t i = 0; i < 256; i++)
    {
      uint16_t rgb444;

      if (i < 16)
        {
          // Indices 0-15: grayscale ramp (0 = black, 15 = white)
          uint16_t gray = i;
          rgb444 = (gray << 8) | (gray << 4) | gray;
        }
      else if (i < 32)
        {
          // Indices 16-31: shades of red (31 = bright red)
          uint16_t intensity = i - 16;
          rgb444 = (intensity << 8) | (0 << 4) | 0;
        }
      else if (i < 48)
        {
          // Indices 32-47: shades of green
          uint16_t intensity = i - 32;
          rgb444 = (0 << 8) | (intensity << 4) | 0;
        }
      else if (i < 64)
        {
          // Indices 48-63: shades of blue
          uint16_t intensity = i - 48;
          rgb444 = (0 << 8) | (0 << 4) | intensity;
        }
      else
        {
          // Indices 64-255: dim grayscale
          uint16_t gray = (i - 64) >> 4;
          rgb444 = (gray << 8) | (gray << 4) | gray;
        }

      palette[i] = rgb444;
    }

  // Clear both hardware buffers to palette index 0 (black).
  memset((void *)VGA_FRAMEBUFFER_BUF0, 0, FRAMEBUFFER_SIZE);
  memset((void *)VGA_FRAMEBUFFER_BUF1, 0, FRAMEBUFFER_SIZE);

  // Enable VSYNC interrupt.
  *irq_enable_reg = VGA_IRQ_VSIEN;
}

//------------------------------------------------------------------------------
// Interrupt setup
//------------------------------------------------------------------------------

void setup_vga_interrupt(void)
{
  // Priority must be >= configMAX_SYSCALL_INTERRUPT_PRIORITY so the ISR
  // can safely call FreeRTOS FromISR API functions.
  NVIC_SetPriority((IRQn_Type)VGA_IRQ, 5);
  NVIC_EnableIRQ((IRQn_Type)VGA_IRQ);
}

void VGA_IRQHandler(void)
{
  vga_isr();
}

//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------

int main(void)
{
  // Platform-specific initialisation (clocks, UART, etc.)
  // TODO: Add your platform init here.

  // Initialise VGA hardware (mode, palette, clear buffers, enable interrupt).
  vga_init();

  // Create the back-buffer index queue using static storage only.
  // xQueueOverwriteFromISR requires the queue length to be exactly 1.
  back_buf_queue = xQueueCreateStatic(1,
                                      sizeof(uint8_t),
                                      back_buf_queue_storage,
                                      &back_buf_queue_cb);

  // xQueueCreateStatic only returns NULL if passed NULL pointers, which
  // cannot happen here; the assert guards against future mistakes.
  configASSERT(back_buf_queue != NULL);

  frame_done_queue = xQueueCreateStatic(1,
                                        sizeof(uint8_t),
                                        frame_done_queue_storage,
                                        &frame_done_queue_cb);
  configASSERT(frame_done_queue != NULL);

  // Pre-load the queue with index 1 so the render task immediately draws
  // into buffer 1 while buffer 0 is displayed.
  uint8_t initial_back = 1;
  xQueueSend(back_buf_queue, &initial_back, 0);

  // Create the render task using static storage only.
  // A single task suffices -- no vga_update_task is needed because the
  // hardware DBR flip replaces the memcpy entirely.
  TaskHandle_t render_handle =
    xTaskCreateStatic(render_task,
                      "Render",
                      RENDER_TASK_STACK_WORDS,
                      NULL,
                      configMAX_PRIORITIES - 2,
                      render_task_stack,
                      &render_task_tcb);

  configASSERT(render_handle != NULL);

  // Enable the VGA VSYNC interrupt before starting the scheduler so the
  // ISR cannot fire before the queue handle is valid.
  setup_vga_interrupt();

  // Start the FreeRTOS scheduler.  Does not return.
  vTaskStartScheduler();

  // Should never reach here.
  for (;;);

  return 0;
}

//------------------------------------------------------------------------------
// FreeRTOS static-allocation callbacks
//
// These are called by the kernel when it needs memory for the idle task and
// the timer daemon task.  We supply pointers to the static storage declared
// at the top of this file.  The kernel never calls pvPortMalloc() for these
// objects, so the heap is not involved at all.
//------------------------------------------------------------------------------

// Called once during vTaskStartScheduler() to get storage for the idle task.
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t  **ppxIdleTaskStackBuffer,
                                   uint32_t      *pulIdleTaskStackSize)
{
  *ppxIdleTaskTCBBuffer   = &idle_task_tcb;
  *ppxIdleTaskStackBuffer = idle_task_stack;
  *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

// Called once during vTaskStartScheduler() to get storage for the timer task.
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimeTaskTCBBuffer,
                                    StackType_t  **ppxTimeTaskStackBuffer,
                                    uint32_t      *pulTimeTaskStackSize)
{
  *ppxTimeTaskTCBBuffer   = &timer_task_tcb;
  *ppxTimeTaskStackBuffer = timer_task_stack;
  *pulTimeTaskStackSize   = configTIMER_TASK_STACK_DEPTH;
}

//------------------------------------------------------------------------------
// Remaining FreeRTOS hook functions
//------------------------------------------------------------------------------

void vApplicationMallocFailedHook(void)
{
  // The application never calls pvPortMalloc(), so this should be
  // unreachable.  Halt here if it fires to aid debugging.
  for (;;);
}

void vAssertCalled(unsigned line, const char * const filename)
{
  (void)line;
  (void)filename;
  unsigned uSetToNonZeroInDebuggerToContinue = 0;
  taskENTER_CRITICAL();
  {
    while (uSetToNonZeroInDebuggerToContinue == 0)
      {
      }
  }
  taskEXIT_CRITICAL();
}

void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
  (void)pxTask;
  (void)pcTaskName;
  unsigned uSetToNonZeroInDebuggerToContinue = 0;
  taskENTER_CRITICAL();
  {
    while (uSetToNonZeroInDebuggerToContinue == 0)
      {
      }
  }
  taskEXIT_CRITICAL();
}
