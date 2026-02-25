#ifndef DOOM_TASK_H
#define DOOM_TASK_H

#include <FreeRTOS.h>


void doom_task(void *pvParameters);

//! I highly doubt this is large enough
#define DOOM_STACK_SIZE 512

/* Structure that will hold the TCB of the task being created. */
extern StaticTask_t doom_TCB;

/* Buffer that the task being created will use as its stack. Note this
is an array of StackType_t variables. The size of StackType_t is
dependent on the RTOS port. */
extern StackType_t doom_stack[ DOOM_STACK_SIZE ];

#endif
