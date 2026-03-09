#include <FreeRTOSConfig.h>
#include <FreeRTOS.h>
#include <task.h>
#include <UART_16550.h>
#include <ANSI_terminal.h>
#include <stats_task.h>
#include <device_addrs.h>
#include <LDP-001_PM_driver.h>
#include <audio_backend.h>
#include <music.h>
#include <stdlib.h>
#include <AXI_timer.h>

// Use the following command to connect from terminal emulator
// "screen /dev/ttyUSB1 57600"

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
  // This is one tick ahead of the compare value that triggered
  // this interrupt.
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
  // the MTIME interrupt will immediately re-trigger after we
  // return.  Since MTIME is IRQ 0 (highest priority in the AXI
  // INTC), this would starve all other interrupts.  Advance
  // MTIMECMP past the current MTIME to prevent this.
  if (now >= cmp) {
    cmp = now + (uint64_t)uxTimerIncrementsForOneTick;
    *pullMachineTimerCompareRegister = cmp;
    ullNextTime = cmp + (uint64_t)uxTimerIncrementsForOneTick;
  }

  if (xTaskIncrementTick() != pdFALSE) {
    vTaskSwitchContext();
  }
}


int main( void )
{
  //  TaskHandle_t hello_handle = NULL;
  TaskHandle_t stats_handle = NULL;
  // TaskHandle_t firework_handle = NULL;
  // TaskHandle_t testcurs_handle = NULL;
  // TaskHandle_t invaders_handle = NULL;
  TaskHandle_t music_producer_handle = NULL;
  TaskHandle_t midi_receiver_handle = NULL;

  // Lower number is higher prority.
  /* INTC_SetPriority(UART0_IRQ,0x6); // priority for UART */
  /* INTC_SetPriority(UART1_IRQ,0x5); // priority for UART */
  /* INTC_SetPriority(GPIO0_IRQ,0x7); */
  /* INTC_SetPriority(PM_IRQ,0x4); */

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
  

  // Set up the AXI timer that is used as MTIME and MTIMECMP.
  // Since portasmHAS_MTIME=0, the FreeRTOS trap handler does NOT
  // handle cause 7 directly.  The MTIME interrupt comes through the
  // AXI INTC as an external interrupt (cause 11).  We dispatch it
  // to mtime_tick_handler via IVAR.
  INTC_SetVector(MTIME_IRQ, mtime_tick_handler);
  INTC_EnableIRQ(MTIME_IRQ);

  INTC_SetVector(UART0_IRQ, UART0_handler);
  INTC_EnableIRQ(UART0_IRQ);

  INTC_SetVector(UART1_IRQ, UART1_handler);
  INTC_EnableIRQ(UART1_IRQ);

  INTC_SetVector(GPIO0_IRQ, button_handler);
  INTC_EnableIRQ(GPIO0_IRQ);

  // Reset PM hardware BEFORE enabling PM_IRQ.  After a GDB reload,
  // PM hardware retains OE/IE/FIFO state from the previous run but
  // driver state (BSS) is zeroed.  If PM_IRQ is enabled while the
  // hardware is still asserting, the ISR runs with NULL handlers
  // and the interrupt never clears → infinite loop.
  PM_reset_hardware();

  INTC_SetVector(PM_IRQ, PM_handler);
  INTC_EnableIRQ(PM_IRQ);

  INTC_SetVector(TIMER0_IRQ, AXI_TIMER_0_ISR);
  INTC_EnableIRQ(TIMER0_IRQ);

  INTC_SetVector(TIMER1_IRQ, AXI_TIMER_1_ISR);
  INTC_EnableIRQ(TIMER1_IRQ);

  INTC_Enable_Global();

  // Intitialize all UARTS
  UART_16550_init();

  // Configure UART 0 for connection to terminal
  UART_16550_configure(UART0,57600,UART_PARITY_NONE,8,1);

  // Standard MIDI baoud rate is 31250
  UART_16550_configure(UART1,31250,UART_PARITY_NONE,8,1);

  // Initialize the Pulse Modulator
  PM_init();

  // Initialize the sound effects code
  audio_init();

  // Initialization for the music_producer and MIDI_receiver
  music_init();
  
  // Seed the PRNG from MTIME XORed with the DIP switches.
  // MTIME provides varying entropy on GDB reloads (counter keeps
  // running).  The DIP switches let the user vary the seed on
  // board resets (where MTIME resets to 0).
  {
    volatile uint32_t *mtime_lo = (volatile uint32_t *)configMTIME_BASE_ADDRESS;
    volatile uint32_t *mtime_hi = (volatile uint32_t *)(configMTIME_BASE_ADDRESS + 4UL);
    srand(*mtime_lo ^ *mtime_hi ^ *SWITCHES);
  }

  /* Create the task without using any dynamic memory allocation. */
  //firework_handle = xTaskCreateStatic(firework_task,"firework",
  //				      FIREWORK_STACK_SIZE,
  // 				      NULL,2,firework_stack,&firework_TCB);
			      
  /* Create the task without using any dynamic memory allocation. */
  //testcurs_handle = xTaskCreateStatic(testcurs_task,"testcurs",
  //				      TESTCURS_STACK_SIZE,
  //				      NULL,3,testcurs_stack,&testcurs_TCB);

  /* Create the task without using any dynamic memory allocation. */
  /* invaders_handle = xTaskCreateStatic(ninvaders,"ninvaders", */
  /* 				      NINVADERS_STACK_SIZE, */
  /* 				      NULL,2,ninvaders_stack,&ninvaders_TCB); */
			      
  music_producer_handle = xTaskCreateStatic(music_producer_task,
                                            "music producer",
                                            MUSIC_PRODUCER_STACK_SIZE,
                                            NULL,10,music_producer_stack,
                                            &music_producer_TCB);
			      
  midi_receiver_handle = xTaskCreateStatic(midi_receiver_task,
					   "midi_receiver",
					   MIDI_RECEIVER_STACK_SIZE,
					   NULL,9,midi_receiver_stack,
					   &midi_receiver_TCB);
  
  stats_handle = xTaskCreateStatic(stats_task,"stats",STATS_STACK_SIZE,
  				   NULL,3,stats_stack,&stats_TCB);

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


void malloc_failed()
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



