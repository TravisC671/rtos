#include <FreeRTOSConfig.h>
#include <FreeRTOS.h>
#include <task.h>
#include <uart.h>
#include <hello_task.h>
#include <stats_task.h>

#define STACK_SIZE 1024
StaticTask_t stats_tcb;
StackType_t stats_stack[STACK_SIZE];

int main()
{
  // configure the uart for 9600/N/8/2
  uart_init(9600);

  xTaskCreateStatic(
      hello_task,
      "hello_task",
      STACK_SIZE,
      NULL,
      4,
      hello_stack,
      &hello_TCB);

  xTaskCreateStatic(
      stats_task,
      "stats_task",
      STACK_SIZE,
      NULL,
      2,
      stats_stack,
      &stats_tcb);
  // create task & start scheduler
  vTaskStartScheduler();

  while (1);
}

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