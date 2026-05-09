
#include <FreeRTOS.h>
#include <task.h>
#include <UART_16550.h>
#include <hello_task.h>
#include <device_addrs.h>
#include <interrupts.h>
#include <AXI_timer.h>
#include <stdio.h>
#include <load_task.h>
#include <sd_driver.h>

/* These globals live in the FreeRTOS RISC-V port (port.c).  The trap
   handler in portASM.S uses them to reload mtimecmp on every tick when
   portasmHAS_MTIME = 1.  We initialize them here so the first tick
   fires exactly one period after the scheduler starts. */
extern volatile uint64_t  *pullMachineTimerCompareRegister;
extern uint64_t            ullNextTime;
extern const size_t        uxTimerIncrementsForOneTick;
extern const uint32_t      ullMachineTimerCompareRegisterBase;

void vPortSetupTimerInterrupt(void)
{
  volatile uint32_t *const mtime_lo  = (volatile uint32_t *)MTIME_TIMER;
  volatile uint32_t *const mtime_hi  = (volatile uint32_t *)MTIME_TIMER + 1;
  uint32_t hi1;
  uint32_t lo;
  uint32_t hi2;
  uint64_t now;

  /* Point the port at our mtimecmp register (single hart, no offset). */
  pullMachineTimerCompareRegister =
    (volatile uint64_t *)ullMachineTimerCompareRegisterBase;

  /* Read the 64-bit mtime atomically on a 32-bit core using the
     standard read-hi / read-lo / re-read-hi pattern. */
  hi2 = *mtime_hi;
  do
    {
      hi1 = hi2;
      lo  = *mtime_lo;
      hi2 = *mtime_hi;
    }
  while (hi1 != hi2);

  now = ((uint64_t)hi1 << 32) | (uint64_t)lo;

  /* Schedule the first tick and stash the following deadline for the
     trap handler to use on the next interrupt. */
  ullNextTime = now + (uint64_t)uxTimerIncrementsForOneTick;
  *pullMachineTimerCompareRegister = ullNextTime;
  ullNextTime += (uint64_t)uxTimerIncrementsForOneTick;

  /* mtimecmp is armed, so it's safe to unmask the machine timer
     interrupt in mie.  mstatus.MIE stays off until the scheduler
     restores the first task. */
  INTC_Enable_MTIMER_interrupt();
}

int main(int argc, char **argv)
{
   TaskHandle_t hello_handle = NULL;
   char buffer[64];

   UART_16550_init();

   UART_16550_configure(UART0,57600,UART_PARITY_NONE,8,1);
   UART_16550_configure(UART1,57600,UART_PARITY_NONE,8,1);

   //Change this to the 14mb for the WAD
   sd_driver_add_dma_region(0x10000000, 0x40000000);
   
   sd_driver_init();

   //hide cursor
   print("\033[?25l");
   
   print("╔══════════════════════════╗\r\n");
   print("║ Initializing Doom Loader ║\r\n");
   print("╚══════════════════════════╝\r\n");
   
   // xTaskCreateStatic(
   //     hello_task,
   //     "hello_task",
   //     HELLO_STACK_SIZE,
   //     NULL,
   //     4,
   //     hello_stack,
   //     &hello_TCB);

   xTaskCreateStatic(load_task, "load",
                     LOAD_STACK_SIZE, NULL, 4,
                     load_stack, &load_TCB);

   vTaskStartScheduler();

   /* we should never get to this point, but if we do, go into infinite
      loop */
   while (1)
   {
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
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize )
{
/* If the buffers to be provided to the Timer task are declared inside
this function then they must be declared static - otherwise they will
be allocated on the stack and so not exists after this function
exits. */
static StaticTask_t xTimerTaskTCB;
static StackType_t uxTimerTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the
    Timer task's state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task's stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulTimerTaskStackSize = configMINIMAL_STACK_SIZE;
}
/*-----------------------------------------------------------*/


/* DIAG: idle hook called on every iteration of prvIdleTask's loop.
 * Detects the MIE-stuck-at-0 mret bug: if MIE is 0 here, the most
 * recent restore failed to leave MIE=1.  Instead of halting, set MIE
 * back on and increment a recovery counter so the system keeps running
 * and we can watch how often the bug fires.  The mstatus value seen
 * BEFORE the recovery is captured so we can post-mortem the exact bit
 * pattern. */
volatile uint32_t idle_hook_count          = 0;
volatile uint32_t idle_hook_last_mstatus   = 0;
volatile uint32_t idle_mie_recovery_count  = 0;
volatile uint32_t idle_mie_recovery_last   = 0;

void vApplicationIdleHook(void)
{
    uint32_t mst;
    asm volatile("csrr %0, mstatus" : "=r"(mst));
    idle_hook_last_mstatus = mst;
    idle_hook_count++;
    if ((mst & 0x8) == 0) {
        idle_mie_recovery_last  = mst;
        idle_mie_recovery_count++;
        /* Force MIE=1 to recover from the hardware mret-MIE bug. */
        asm volatile("csrsi mstatus, 8");
    }
}


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
