
#include <FreeRTOS.h>
#include <task.h>
#include <UART_16550.h>
#include <sd_test.h>
#include <device_addrs.h>
#include <interrupts.h>

// "screen /dev/ttyUSB1 9600"

// Variables defined in FreeRTOS port.c for MTIME handling
extern uint64_t ullNextTime;
extern const size_t uxTimerIncrementsForOneTick;
extern volatile uint64_t *pullMachineTimerCompareRegister;

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
  cmp = ullNextTime;
  *pullMachineTimerCompareRegister = cmp;
  ullNextTime = cmp + (uint64_t)uxTimerIncrementsForOneTick;

  // Read current MTIME (atomic 64-bit read on 32-bit RISC-V).
  do {
    hi = *pulTimeHigh;
    lo = *pulTimeLow;
  } while (hi != *pulTimeHigh);
  now = ((uint64_t)hi << 32) | (uint64_t)lo;

  // If MTIME has already passed the compare value we just wrote,
  // advance MTIMECMP past current MTIME to prevent interrupt storm.
  if (now >= cmp) {
    cmp = now + (uint64_t)uxTimerIncrementsForOneTick;
    *pullMachineTimerCompareRegister = cmp;
    ullNextTime = cmp + (uint64_t)uxTimerIncrementsForOneTick;
  }

  if (xTaskIncrementTick() != pdFALSE) {
    vTaskSwitchContext();
  }
}

/* Dimensions the buffer that the task being created will use as its
stack. NOTE: This is the number of words the stack will hold, not the
number of bytes. For example, if each stack item is 32-bits, and this
is set to 100, then 400 bytes (100 * 32-bits) will be allocated. */
#define STACK_SIZE 2048

/* Structure that will hold the TCB of the task being created. */
StaticTask_t sd_test_TCB;

/* Buffer that the task being created will use as its stack. Note this
is an array of StackType_t variables. The size of StackType_t is
dependent on the RTOS port. */
StackType_t sd_test_stack[ STACK_SIZE ];

int main( void )
{
  TaskHandle_t sd_test_handle = NULL;
  volatile uint64_t *mtimecmp;

  // Reset the INTC to a known state.  The AXI INTC is not reset when
  // loading via the debugger, so stale IER/IVAR from a previous run
  // can cause spurious interrupts with garbage handler addresses.
  INTC_Init();

  // After a GDB reload, MTIME has been counting since power-on but
  // MTIMECMP retains a stale (old) value.  MTIME > MTIMECMP, so the
  // MTIME interrupt fires as soon as global interrupts are enabled.
  // Set MTIMECMP to max so MTIME won't fire until the scheduler sets
  // it properly.
  mtimecmp = (volatile uint64_t *)configMTIMECMP_BASE_ADDRESS;
  *mtimecmp = 0xFFFFFFFFFFFFFFFFULL;

  // Register the MTIME tick handler with the AXI INTC.
  // Since portasmHAS_MTIME=0, the FreeRTOS trap handler does NOT
  // handle cause 7 directly.  The MTIME interrupt comes through the
  // AXI INTC as an external interrupt (cause 11).
  INTC_SetVector(MTIME_IRQ, mtime_tick_handler);
  INTC_EnableIRQ(MTIME_IRQ);

  // Register the UART interrupt handlers
  extern void UART0_handler(void);
  extern void UART1_handler(void);
  INTC_SetVector(UART0_IRQ, UART0_handler);
  INTC_EnableIRQ(UART0_IRQ);
  INTC_SetVector(UART1_IRQ, UART1_handler);
  INTC_EnableIRQ(UART1_IRQ);

  // Enable the INTC master enable
  INTC_Enable_Global();

  // Initialize the UART driver (creates stream buffers and mutexes)
  UART_16550_init();
  // Configure UART0 for 9600/N/8/2
  UART_16550_configure(UART0, 9600, UART_PARITY_NONE, 8, 1);

  /* Create the SD test task without using any dynamic memory allocation. */
  sd_test_handle = xTaskCreateStatic(sd_test_task,"sd_test",STACK_SIZE,
				     NULL,2,sd_test_stack,&sd_test_TCB);

  /* start the scheduler */
  vTaskStartScheduler();

  /* we should never get to this point, but if we do, go into infinite
     loop */
  while(1);
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

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    while(1);
}
