//------------------------------------------------------------------------------
// Copyright (c) 2026 Larry D. Pyeatt
// All rights reserved.
//------------------------------------------------------------------------------
// Main Application - VGA Controller with FreeRTOS (640x400 Mode)
//------------------------------------------------------------------------------
#include <device_addrs.h>
#include <vga_driver.h>
#include <core_cm3.h>
#include "asm_string.h"

//------------------------------------------------------------------------------
// Global Variables
//------------------------------------------------------------------------------

// Binary semaphores for task synchronization
SemaphoreHandle_t vsync_semaphore = NULL;
SemaphoreHandle_t draw_frame_semaphore = NULL;

// Shared video buffer pointer (allocated from heap during initialization)
uint8_t *video_buffer = NULL;

//------------------------------------------------------------------------------
// VGA Hardware Initialization
//------------------------------------------------------------------------------

void vga_init(void)
{
  volatile uint32_t *control_reg     = (volatile uint32_t *)VGA_CONTROL_REG;
  volatile uint32_t *irq_enable_reg  = (volatile uint32_t *)VGA_IRQ_ENABLE_REG;
    
  // Allocate video buffer from heap (256000 bytes for 640x400)
  video_buffer = (uint8_t *)pvPortMalloc(FRAMEBUFFER_SIZE);
    
  // Check allocation success
  if (video_buffer == NULL)
    {
      // Allocation failed - heap too small
      // Increase configTOTAL_HEAP_SIZE in FreeRTOSConfig.h
      for (;;)
        {
	  // Halt here - debug breakpoint location
        }
    }
    
  // Configure VGA for 640x400 mode (bit 0 = 0)
  *control_reg = VGA_CTRL_MODE_640x400;
    
  // Initialize palette.
  // The hardware packs two 12-bit RGB entries per 32-bit word:
  //   bits [11: 0]  even entry (colour index 2n)
  //   bits [27:16]  odd  entry (colour index 2n+1)
  //   bits [31:28] and [15:12] are unused (RAZ/WI -- masked on write by hardware)
  // We must use 32-bit word writes; sub-word (halfword/byte) writes would
  // trigger the AXI RMW path which is unreliable for distributed (LUT) RAM.
  volatile uint32_t *palette32 = (volatile uint32_t *)VGA_PALETTE_BASE;
  for (uint32_t i = 0; i < 128; i++)
    {
      uint32_t idx_even = i * 2;
      uint32_t idx_odd  = i * 2 + 1;
      uint16_t even_rgb, odd_rgb;

      // Even entry
      if (idx_even < 16)
        { uint16_t g = idx_even; even_rgb = (g<<8)|(g<<4)|g; }
      else if (idx_even < 32)
        { uint16_t v = idx_even-16; even_rgb = (v<<8); }
      else if (idx_even < 48)
        { uint16_t v = idx_even-32; even_rgb = (v<<4); }
      else if (idx_even < 64)
        { uint16_t v = idx_even-48; even_rgb = v; }
      else
        { uint16_t g = (idx_even-64)>>4; even_rgb = (g<<8)|(g<<4)|g; }

      // Odd entry
      if (idx_odd < 16)
        { uint16_t g = idx_odd; odd_rgb = (g<<8)|(g<<4)|g; }
      else if (idx_odd < 32)
        { uint16_t v = idx_odd-16; odd_rgb = (v<<8); }
      else if (idx_odd < 48)
        { uint16_t v = idx_odd-32; odd_rgb = (v<<4); }
      else if (idx_odd < 64)
        { uint16_t v = idx_odd-48; odd_rgb = v; }
      else
        { uint16_t g = (idx_odd-64)>>4; odd_rgb = (g<<8)|(g<<4)|g; }

      // Pack: even in bits[11:0], odd in bits[27:16]
      palette32[i] = ((uint32_t)(odd_rgb & 0xFFF) << 16) | (even_rgb & 0xFFF);
    }
    
  // Clear the VGA framebuffer using STMIA burst writes.
  // VGA_FRAMEBUFFER_BASE is word-aligned and FRAMEBUFFER_SIZE (256000) is
  // divisible by 32, so vga_clear_buffer_asm goes straight into the burst loop.
  vga_clear_buffer_asm((void *)VGA_FRAMEBUFFER_BASE, 0, FRAMEBUFFER_SIZE);

  // Clear the shared video buffer (heap allocation is at least 4-byte aligned
  // and 256000 is divisible by 32, so the burst loop applies here too).
  vga_clear_buffer_asm(video_buffer, 0, FRAMEBUFFER_SIZE);

  // Enable VSYNC interrupt.  Writing to VGA_IRQ_ENABLE_REG is a one-time
  // operation at init; the ISR only touches VGA_IRQ_CLEAR_REG to acknowledge.
  *irq_enable_reg = VGA_IRQ_VSIEN;
}

//------------------------------------------------------------------------------
// Platform-Specific Interrupt Setup - Cortex-M3
//------------------------------------------------------------------------------

void setup_vga_interrupt(void)
{
  // Set interrupt priority (must be >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY)
  // Using priority 5 as an example - adjust based on your FreeRTOSConfig.h
  NVIC_SetPriority((IRQn_Type)VGA_IRQ, 5);
    
  // Enable the VGA interrupt in NVIC
  NVIC_EnableIRQ((IRQn_Type)VGA_IRQ);
}

// VGA IRQ Handler - called by NVIC
// This must be declared in your startup file's vector table
void VGA_IRQHandler(void)
{
  vga_isr();
}

//------------------------------------------------------------------------------
// Main Function
//------------------------------------------------------------------------------

int main(void)
{
  // Platform-specific initialization (clocks, UART, etc.)
  // TODO: Add your platform init here
    
  // Initialize VGA hardware (allocates 256KB video buffer from heap)
  vga_init();
    
  // Verify video buffer allocation succeeded
  if (video_buffer == NULL)
    // Should never reach here if vga_init() checks properly
    for (;;);
    
  // Create binary semaphores
  vsync_semaphore = xSemaphoreCreateBinary();
  draw_frame_semaphore = xSemaphoreCreateBinary();
    
  // Check semaphore creation
  if (vsync_semaphore == NULL || draw_frame_semaphore == NULL)
    {
      // Semaphore creation failed - handle error
      for (;;)
        {
	  // Halt here - check heap size in FreeRTOSConfig.h
        }
    }
    
  // Give initial semaphore to render task so it can start rendering
  xSemaphoreGive(draw_frame_semaphore);
    
  // Create VGA update task (high priority - must run every VSYNC)
  xTaskCreate(vga_update_task,
	      "VGA_Update",
	      256,                    // Stack size (adjust as needed)
	      NULL,
	      configMAX_PRIORITIES - 1,  // High priority
	      NULL);
    
  // Create render task (lower priority - can be preempted by VGA update task)
  xTaskCreate(render_task,
	      "Render",
	      512,                    // Stack size (adjust as needed)
	      NULL,
	      configMAX_PRIORITIES - 2,  // One below VGA update so VSYNC preempts render
	      NULL);
    
  // Setup platform-specific interrupt for VGA controller
  setup_vga_interrupt();
    
  // Start the FreeRTOS scheduler
  vTaskStartScheduler();
    
  // Should never reach here
  for (;;)
    {
      // Scheduler failed to start - check heap/stack configuration
    }
    
  return 0;
}

//------------------------------------------------------------------------------
// FreeRTOS Hook Functions (Optional)
//------------------------------------------------------------------------------

// Malloc failed hook (if enabled in FreeRTOSConfig.h)
void vApplicationMallocFailedHook(void)
{
  // Memory allocation failed
  for (;;)
    {
      // Debug: Set breakpoint here
      // Consider increasing configTOTAL_HEAP_SIZE
    }
}


/* Blatantly stolen from
   https://www.freertos.org/a00110.html#include_parameters
   and I really don't understand it yet.
*/

/* configSUPPORT_STATIC_ALLOCATION is set to 1, so the application
   must provide an implementation of vApplicationGetIdleTaskMemory() to
   provide the memory that is used by the Idle task. */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{
  /* If the buffers to be provided to the Idle task are declared inside
     this function then they must be declared static - otherwise they will
     be allocated on the stack and so not exists after this function
     exits. */
  static StaticTask_t xIdleTaskTCB;
  static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

  /* Pass out a pointer to the StaticTask_t structure in which the
     Idle task's state will be stored. */
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

  /* Pass out the array that will be used as the Idle task's stack. */
  *ppxIdleTaskStackBuffer = uxIdleTaskStack;

  /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
     Note that, as the array is necessarily of type StackType_t,
     configMINIMAL_STACK_SIZE is specified in words, not bytes. */
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/*-----------------------------------------------------------*/



/* configSUPPORT_STATIC_ALLOCATION is set to 1, so the application
   must provide an implementation of vApplicationGetTimerTaskMemory() to
   provide the memory that is used by the Timer task. */
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimeTaskTCBBuffer,
				     StackType_t **ppxTimeTaskStackBuffer,
				     uint32_t *pulTimeTaskStackSize )
{
  /* If the buffers to be provided to the Time task are declared inside
     this function then they must be declared static - otherwise they will
     be allocated on the stack and so not exists after this function
     exits. */
  static StaticTask_t xTimeTaskTCB;
  static StackType_t uxTimeTaskStack[ configTIMER_TASK_STACK_DEPTH ];

  /* Pass out a pointer to the StaticTask_t structure in which the
     Time task's state will be stored. */
  *ppxTimeTaskTCBBuffer = &xTimeTaskTCB;

  /* Pass out the array that will be used as the Time task's stack. */
  *ppxTimeTaskStackBuffer = uxTimeTaskStack;

  /* Pass out the size of the array pointed to by *ppxTimeTaskStackBuffer.
     Note that, as the array is necessarily of type StackType_t,
     configMINIMAL_STACK_SIZE is specified in words, not bytes. */
  *pulTimeTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
/*-----------------------------------------------------------*/

void vAssertCalled( unsigned line, const char * const filename )
{
  unsigned uSetToNonZeroInDebuggerToContinue=0;
  taskENTER_CRITICAL();
  {
    /* You can step out of this function to debug the assertion by using
       the debugger to set ulSetToNonZeroInDebuggerToContinue to a non-zero
       value. */
    while(uSetToNonZeroInDebuggerToContinue == 0)
      {
      }
  }
  taskEXIT_CRITICAL();
}

/*-----------------------------------------------------------*/


/* void malloc_failed() */
/* { */
/*   unsigned uSetToNonZeroInDebuggerToContinue=0; */
/*     taskENTER_CRITICAL(); */
/*     { */
/*         /\* You can step out of this function to debug the assertion by using */
/*         the debugger to set ulSetToNonZeroInDebuggerToContinue to a non-zero */
/*         value. *\/ */
/*         while(uSetToNonZeroInDebuggerToContinue == 0) */
/*         { */
/*         } */
/*     } */
/*     taskEXIT_CRITICAL(); */
/* } */



/*-----------------------------------------------------------*/


void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName )
{
  unsigned uSetToNonZeroInDebuggerToContinue=0;
  taskENTER_CRITICAL();
  {
    /* You can step out of this function to debug the assertion by using
       the debugger to set ulSetToNonZeroInDebuggerToContinue to a non-zero
       value. */
    while(uSetToNonZeroInDebuggerToContinue == 0)
      {
      }
  }
  taskEXIT_CRITICAL();
}


