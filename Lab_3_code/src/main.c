
#include <FreeRTOS.h>
#include <task.h>
#include <UART_16550.h>
#include <hello_task.h>
#include <stats_task.h>
#include <device_addrs.h>

// "screen /dev/ttyUSB1 9600"


int main( void )
{
  TaskHandle_t hello_handle = NULL;
  TaskHandle_t stats_handle = NULL;

  NVIC_SetPriority(UART0_IRQ,0x6); // priority for UART
  NVIC_SetPriority(UART1_IRQ,0x6); // priority for UART

  // Intitialize all UARTS
  UART_16550_init();

  // Configure UART0 for 9600/N/8/2
  UART_16550_configure(UART0,9600,UART_PARITY_NONE,8,2);
  UART_16550_configure(UART1,9600,UART_PARITY_NONE,8,2);
  
  /* Create the task without using any dynamic memory allocation. */
  hello_handle = xTaskCreateStatic(hello_task,"hello",HELLO_STACK_SIZE,
				   NULL,3,hello_stack,&hello_TCB);
			      
  /* Create the task without using any dynamic memory allocation. */
  stats_handle = xTaskCreateStatic(stats_task,"stats",STATS_STACK_SIZE,
				   NULL,2,stats_stack,&stats_TCB);
			      
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
