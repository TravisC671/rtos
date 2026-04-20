#ifndef STATS_TASK_H
#define STATS_TASK_H

#include <FreeRTOS.h>

// "screen /dev/ttyUSB1 9600"

void stats_task(void *pvParameters);

/* Dimensions the buffer that the task being created will use as its
stack. NOTE: This is the number of words the stack will hold, not the
number of bytes. For example, if each stack item is 32-bits, and this
is set to 100, then 400 bytes (100 * 32-bits) will be allocated. */
#define STATS_STACK_SIZE 256

/* Structure that will hold the TCB of the task being created. */
extern StaticTask_t stats_TCB;

/* Buffer that the task being created will use as its stack. Note this
is an array of StackType_t variables. The size of StackType_t is
dependent on the RTOS port. */
extern StackType_t stats_stack[ STATS_STACK_SIZE ];

#endif
