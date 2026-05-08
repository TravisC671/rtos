#ifndef LOAD_TASK_H
#define LOAD_TASK_H

#include <FreeRTOSConfig.h>
#include <FreeRTOS.h>
#include <task.h>


void load_task(void *pvParameters);

#define LOAD_STACK_SIZE 4096

extern StaticTask_t load_TCB;

extern StackType_t load_stack[ LOAD_STACK_SIZE ];

#endif