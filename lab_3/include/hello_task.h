#ifndef HELLO_TASK_H
#define HELLO_TASK_H

#include <FreeRTOS.h>

// "screen /dev/ttyUSB1 9600"

void hello_task(void *pvParameters);

#define HELLO_STACK_SIZE 256

extern StaticTask_t hello_TCB;

extern StackType_t hello_stack[ HELLO_STACK_SIZE ];

#endif