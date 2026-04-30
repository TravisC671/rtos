#include <stdio.h>
#include <UART_16550.h>
#include <load_task.h>
#include "ff_stdio.h"
#include "ff_headers.h"

void load_task(void *pvParameters)
{
    char buffer[160];
    FF_FindData_t xFind;
    int rc;

    sprintf(buffer, "Starting Read from SD Card\r\n");

    UART_16550_write_string(UART0, buffer, portMAX_DELAY);

    rc = ff_findfirst("/sd/", &xFind);

    if (rc != 0)
    {
        sprintf(buffer, "ff_findfirst failed: %d\r\n", rc);

        UART_16550_write_string(UART0, buffer, portMAX_DELAY);
    }
    else
    {
        do
        {
            sprintf(buffer, "    %-20s %8lu %s\r\n",
                    xFind.pcFileName,
                    (unsigned long)xFind.ulFileSize,
                    (xFind.ucAttributes & FF_FAT_ATTR_DIR) ? "<DIR>" : "");

            UART_16550_write_string(UART0, buffer, portMAX_DELAY);
        } while (ff_findnext(&xFind) == 0);
    }
    while (1)
    {
    }
}

StaticTask_t load_TCB;

StackType_t load_stack[LOAD_STACK_SIZE];