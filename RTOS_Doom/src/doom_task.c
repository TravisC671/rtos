
#include <doom_task.h>
#include <FreeRTOSConfig.h>
#include <FreeRTOS.h>
#include <task.h>
#include <doomgeneric_freertos.h>
#include <doomgeneric.h>
#include <task.h>
#include <UART_16550.h>
#include <stdio.h>
#include <ff_stdio.h>
#include "ff_headers.h"
#include "sd_driver.h"
#include "sd_fat_disk.h"

static int stats_counter = 0;

static FF_Disk_t *mount_fs(void)
{
    FF_Error_t xErr;

    FF_Disk_t *pxDisk;
    int mounted;

    pxDisk = sd_fat_disk_init();
    if (pxDisk == NULL)
    {
        uart0_print("sd_fat_disk_init returned NULL\r\n");
        return NULL;
    }

    // Check if the partition is actually mounted
    mounted = (pxDisk->pxIOManager != NULL &&
               pxDisk->pxIOManager->xPartition.ulTotalSectors != 0);

    if (mounted)
    {
        uart0_print("Mounted SD Card\r\n");
        return pxDisk;
    }
    else
    {
        uart0_print("Could not mount SD Card\r\n");
        return NULL;
    }
}


void doom_task(void *pvParameters)
{
    FF_Disk_t *pxDisk;
    char buffer[50];

    // get user input later
    char *argv[] = {"doom", "-iwad", "/sd/doom1.wad", NULL};
    int argc = sizeof(argv) / sizeof(argv[0]) - 1;

    uart0_print("\033[H\033[2J");
    //ascii art from https://www.gamers.org/dhs/helpdocs/dmsp1666.html
    uart0_print("=================     ===============     ===============   ================\r\n");
    uart0_print("\\\\ . . . . . . .\\\\   //. . . . . . .\\\\   //. . . . . . .\\\\  \\\\. . .\\\\// . .//\r\n");
    uart0_print("||. . ._____. . .|| ||. . ._____. . .|| ||. . ._____. . .|| || . . .\\/ . ..||\r\n");
    uart0_print("|| . .||   ||. . || || . .||   ||. . || || . .||   ||. . || ||. . . . . . .||\r\n");
    uart0_print("||. . ||   || . .|| ||. . ||   || . .|| ||. . ||   || . .|| || . | . . . ..||\r\n");
    uart0_print("|| . .||   ||. _-|| ||-_ .||   ||. . || || . .||   ||. _-|| ||-_.|\\ . . . .||\r\n");
    uart0_print("||. . ||   ||-'  || ||  `-||   || . .|| ||. . ||   ||-'  || ||  `|\\_ . .|..||\r\n");
    uart0_print("|| . _||   ||    || ||    ||   ||_ . || || . _||   ||    || ||   |\\ `-_/| .||\r\n");
    uart0_print("||_-' ||  .|/    || ||    \\|.  || `-_|| ||_-' ||  .|/    || ||   | \\  / -_.||\r\n");
    uart0_print("||    ||_-'      || ||      `-_||    || ||    ||_-'      || ||   | \\  / | '||\r\n");
    uart0_print("||    `'         || ||         `'    || ||    `'         || ||   | \\  / |  ||\r\n");
    uart0_print("||            .===' `===.         .==='.`===.         .===' /==. |  \\/  |  ||\r\n");
    uart0_print("||         .=='   \\_|-_ `===. .==='   _|_   `===. .===' _-|/   `==  \\/  |  ||\r\n");
    uart0_print("||      .=='    _-'    `-_  `='    _-'   `-_    `='  _-'   `-_  /|  \\/  |  ||\r\n");
    uart0_print("||   .=='    _-'          `-__\\._-'         `-_./__-'         `' |. /|  |  ||\r\n");
    uart0_print("||.=='    _-'                                                     `' | /==.||\r\n");
    uart0_print("=='    _-'           F          P          G          A               \\/  `==\r\n");
    uart0_print("\\   _-'                                                                `-_  /\r\n");
    uart0_print(" `''                                                                      ``'\r\n");

    uart0_print("running doom with arguments: ");

    for (int i = 0; i < argc; i++)
    {
        sprintf(buffer, "%s ", argv[i]);

        uart0_print(buffer);
    }
    uart0_print("\r\n");

    // mount the SD card first
    xSemaphoreTake(sd_semaphore, portMAX_DELAY);

    vTaskDelay(pdMS_TO_TICKS(3000));

    uart0_print("Mounting SD Card\r\n");

    pxDisk = mount_fs();
    if (pxDisk == NULL)
    {
        return;
    }

    doomgeneric_Create(argc, argv);

    while (1)
    {
        // doomgeneric_Tick();
    }
}

/* Structure that will hold the TCB of the task being created. */
StaticTask_t doom_TCB;

/* Buffer that the task being created will use as its stack. Note this
is an array of StackType_t variables. The size of StackType_t is
dependent on the RTOS port. */
StackType_t doom_stack[DOOM_STACK_SIZE];
