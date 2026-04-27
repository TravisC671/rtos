

#include <hello_task.h>
#include <task.h>
#include <uart.h>
#include <stdio.h>


/* Structure that will hold the TCB of the task being created. */
StaticTask_t hello_TCB;

/* Buffer that the task being created will use as its stack. Note this
is an array of StackType_t variables. The size of StackType_t is
dependent on the RTOS port. */
StackType_t hello_stack[ HELLO_STACK_SIZE ];

void hello_task(void *pvParameters)
{
  char buffer[32];
  uint32_t ticks;
  while(1)
    {
      ticks = xTaskGetTickCount();
      sprintf(buffer,"Hello World %10ld\n\r",ticks);
      uart_write_string(buffer);
      vTaskDelay(pdMS_TO_TICKS( 100 ));
    }
}

