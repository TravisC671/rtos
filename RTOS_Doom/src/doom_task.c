
#include <doom_task.h>
#include <doomgeneric.h>
#include <task.h>

static int stats_counter = 0;

void doom_task(void *pvParameters)
{
    // make sure to fix these
    // int argc, char **argv
    doomgeneric_Create();

    while (1)
    {
        doomgeneric_Tick();
    }
}

/* Structure that will hold the TCB of the task being created. */
StaticTask_t doom_TCB;

/* Buffer that the task being created will use as its stack. Note this
is an array of StackType_t variables. The size of StackType_t is
dependent on the RTOS port. */
StackType_t doom_stack[DOOM_STACK_SIZE];
