#include <FreeRTOSConfig.h>
#include <FreeRTOS.h>
#include <task.h>
#include <UART_16550.h>
#include <device_addrs.h>
#include <AXI_timer.h>
#include <hello_task.h>
#include <stdio.h>

// "screen /dev/ttyUSB1 57600"
// Variables defined in FreeRTOS port.c for MTIME handling
extern uint64_t ullNextTime;
extern const size_t uxTimerIncrementsForOneTick;
extern volatile uint64_t *pullMachineTimerCompareRegister;

#define STACK_SIZE 1024

// Tick handler called from freertos_risc_v_application_interrupt_handler
// via the INTC dispatch.  Updates MTIMECMP, increments the tick, and
// requests a context switch if needed.
void mtime_tick_handler(void)
{
  volatile uint32_t *pulTimeHigh;
  volatile uint32_t *pulTimeLow;
  uint32_t hi, lo;
  uint64_t now, cmp;

  pulTimeHigh = (volatile uint32_t *)(configMTIME_BASE_ADDRESS + 4UL);
  pulTimeLow = (volatile uint32_t *)(configMTIME_BASE_ADDRESS);

  // Set MTIMECMP to the pre-calculated next compare value.
  // This is one tick ahead of the compare value that triggered
  // this interrupt.
  cmp = ullNextTime;
  *pullMachineTimerCompareRegister = cmp;
  ullNextTime = cmp + (uint64_t)uxTimerIncrementsForOneTick;

  // Read current MTIME (atomic 64-bit read on 32-bit RISC-V).
  do
  {
    hi = *pulTimeHigh;
    lo = *pulTimeLow;
  } while (hi != *pulTimeHigh);
  now = ((uint64_t)hi << 32) | (uint64_t)lo;

  // If MTIME has already passed the compare value we just wrote,
  // the MTIME interrupt will immediately re-trigger after we
  // return.  Since MTIME is IRQ 0 (highest priority in the AXI
  // INTC), this would starve all other interrupts.  Advance
  // MTIMECMP past the current MTIME to prevent this.
  if (now >= cmp)
  {
    cmp = now + (uint64_t)uxTimerIncrementsForOneTick;
    *pullMachineTimerCompareRegister = cmp;
    ullNextTime = cmp + (uint64_t)uxTimerIncrementsForOneTick;
  }

  if (xTaskIncrementTick() != pdFALSE)
  {
    vTaskSwitchContext();
  }
}

#define STACK_SIZE 1024

int main(int argc, char **argv)
{
  TaskHandle_t hello_handle = NULL;

  // Reset the INTC to a known state.  The AXI INTC is not reset when
  // loading via the debugger, so stale IER/IVAR from a previous run
  // can cause spurious interrupts with garbage handler addresses.
  INTC_Init();

  // After a GDB reload, MTIME has been counting since power-on but
  // MTIMECMP retains a stale (old) value.  MTIME > MTIMECMP, so the
  // MTIME interrupt fires as soon as global interrupts are enabled.
  // That calls xTaskIncrementTick() before vTaskStartScheduler() has
  // initialized FreeRTOS data structures → crash.  Set MTIMECMP to
  // max so MTIME won't fire until the scheduler sets it properly.

  volatile uint64_t *mtimecmp;
  mtimecmp = (volatile uint64_t *)configMTIMECMP_BASE_ADDRESS;
  *mtimecmp = 0xFFFFFFFFFFFFFFFFULL;

  INTC_Enable_Global();

  // Intitialize all UARTS
  UART_16550_init();

  // Configure UART 0 for connection to terminal
  UART_16550_configure(UART0, 9600, UART_PARITY_NONE, 8, 1);

  char buffer[64];

  sprintf(buffer,"Hello World");

  UART_16550_write_string(UART0, buffer, portMAX_DELAY);

  xTaskCreateStatic(
      hello_task,
      "hello_task",
      STACK_SIZE,
      NULL,
      4,
      hello_stack,
      &hello_TCB);

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
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
  /* If the buffers to be provided to the Idle task are declared inside
  this function then they must be declared static - otherwise they will
  be allocated on the stack and so not exists after this function
  exits. */
  static StaticTask_t xIdleTaskTCB;
  static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

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
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimeTaskTCBBuffer,
                                    StackType_t **ppxTimeTaskStackBuffer,
                                    uint32_t *pulTimeTaskStackSize)
{
  /* If the buffers to be provided to the Time task are declared inside
  this function then they must be declared static - otherwise they will
  be allocated on the stack and so not exists after this function
  exits. */
  static StaticTask_t xTimeTaskTCB;
  static StackType_t uxTimeTaskStack[configTIMER_TASK_STACK_DEPTH];

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

void vAssertCalled(unsigned line, const char *const filename)
{
  unsigned uSetToNonZeroInDebuggerToContinue = 0;
  taskENTER_CRITICAL();
  {
    /* You can step out of this function to debug the assertion by using
    the debugger to set ulSetToNonZeroInDebuggerToContinue to a non-zero
    value. */
    while (uSetToNonZeroInDebuggerToContinue == 0)
    {
    }
  }
  taskEXIT_CRITICAL();
}

void malloc_failed()
{
  unsigned uSetToNonZeroInDebuggerToContinue = 0;
  taskENTER_CRITICAL();
  {
    /* You can step out of this function to debug the assertion by using
    the debugger to set ulSetToNonZeroInDebuggerToContinue to a non-zero
    value. */
    while (uSetToNonZeroInDebuggerToContinue == 0)
    {
    }
  }
  taskEXIT_CRITICAL();
}

/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
  unsigned uSetToNonZeroInDebuggerToContinue = 0;
  taskENTER_CRITICAL();
  {
    /* You can step out of this function to debug the assertion by using
    the debugger to set ulSetToNonZeroInDebuggerToContinue to a non-zero
    value. */
    while (uSetToNonZeroInDebuggerToContinue == 0)
    {
    }
  }
  taskEXIT_CRITICAL();
}