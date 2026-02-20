

#include <hello_task.h>
#include <task.h>
#include <UART_16550.h>
#include <stdio.h>

// "screen /dev/ttyUSB1 9600"

void hello_task(void *pvParameters)
{
  char buffer[64];
  uint32_t ticks,last_tick=0,time,min_time=1<<31,max_time=0,max_jitter=0,loop_times=0;
  const TickType_t period = pdMS_TO_TICKS(100);
  
  TickType_t lastwake = xTaskGetTickCount();
  while(1)
    {
      // wait until the timeout
      vTaskDelayUntil(&lastwake,period);
      // vTaskDelay(period);

      // Calculate ticks since we last woke up, and track the jitter
      ticks = xTaskGetTickCount();
      if(loop_times>2)
	{
	  time = ticks-last_tick;
	  if(time < min_time)
	    min_time = time;
	  if(time > max_time)
	    max_time = time;
	  max_jitter = max_time-min_time;
	}
      loop_times++;
      sprintf(buffer,"Hello World %10lu %10lu %10lu\n\r",max_time,min_time,max_jitter);
      // acquire uart 
      UART_16550_write_string(UART0,buffer,portMAX_DELAY);
      // release uart

      last_tick = ticks;
    }
}

/* Dimensions the buffer that the task being created will use as its
stack. NOTE: This is the number of words the stack will hold, not the
number of bytes. For example, if each stack item is 32-bits, and this
is set to 100, then 400 bytes (100 * 32-bits) will be allocated. */
#define STACK_SIZE 256

/* Structure that will hold the TCB of the task being created. */
StaticTask_t hello_TCB;

/* Buffer that the task being created will use as its stack. Note this
is an array of StackType_t variables. The size of StackType_t is
dependent on the RTOS port. */
StackType_t hello_stack[ HELLO_STACK_SIZE ];

