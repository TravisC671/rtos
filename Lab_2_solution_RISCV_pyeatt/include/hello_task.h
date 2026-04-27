#ifndef HELLO_TASK_H
#define HELLO_TASK_H

#include <FreeRTOS.h>

/*  Use a define to set the number of items in the stack for this task.
    NOTE: This is the number of words the stack will hold, not the
    number of bytes. For example, if each stack item is 32-bits, and
    this is set to 100, then 400 bytes (100 * 32-bits) will be
    allocated. */
#define HELLO_STACK_SIZE 2048

/*  Structure that will hold the TCB of the task being created. It is
    declareed external in the header so that space is not allocated.
    The corresponding .c file does the actuall allocation.  We put this
    external declaration here so that other code can initialize and
    start instances of this task. */
extern StaticTask_t hello_TCB;

/*  Buffer that the task being created will use as its stack. Note this
    is an array of StackType_t variables. The size of StackType_t is
    dependent on the RTOS port.  Again,, we must declare it external in
    the header file, but not in the corresponding C file */
extern StackType_t hello_stack[ HELLO_STACK_SIZE ];

/*  Provide prototype for the task function */
void hello_task(void *pvParameters);


#endif
