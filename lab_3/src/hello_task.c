#include <hello_task.h>

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
      vTaskDelay(period);

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

#define STACK_SIZE 256

StaticTask_t hello_TCB;

StackType_t hello_stack[ HELLO_STACK_SIZE ];
