#include <UART_16550.h>
#include <load_task.h>
#include "ff_stdio.h"
#include "ff_headers.h"
#include "sd_driver.h"
#include "sd_fat_disk.h"

#define CHUNK_SIZE 1024

extern uint8_t __wad_start;
extern uint8_t __doom_start;

typedef void (*entry_point_t)(void);

void jump_to_application(void *address)
{
    // 1. Disable Global Interrupts
    asm volatile("csrci mstatus, 8"); // Clear MIE bit

    // 2. Clear all interrupt enable bits
    asm volatile("csrw mie, zero");

    // 3. Instruction Synchronization Barrier
    // This is critical for RISC-V after loading code into RAM
    asm volatile("fence.i");

    // 4. Create the function pointer
    entry_point_t start_app = (entry_point_t)address;

    start_app();
}

static FF_Disk_t *mount_fs(void)
{
    FF_Error_t xErr;

    FF_Disk_t *pxDisk;
    int mounted;

    pxDisk = sd_fat_disk_init();
    if (pxDisk == NULL)
    {
        print("sd_fat_disk_init returned NULL\r\n");
        return NULL;
    }

    // Check if the partition is actually mounted
    mounted = (pxDisk->pxIOManager != NULL &&
               pxDisk->pxIOManager->xPartition.ulTotalSectors != 0);

    if (mounted)
    {
        print("Mounted SD Card\r\n");
        return pxDisk;
    }
    else
    {
        print("Could not mount SD Card\r\n");
        return NULL;
    }
}

static void update_loading_bar(unsigned long cur, unsigned long max)
{
    const int BAR_WIDTH = 20;
    char buffer[50];

    int percentage = (int)((cur * 100) / max);

    int filled_width = (int)((cur * BAR_WIDTH) / max);

    print("\rProgress: [");

    for (int i = 0; i < BAR_WIDTH; i++)
    {
        if (i < filled_width)
        {
            print("=");
        }
        else if (i == filled_width)
        {
            print(">");
        }
        else
        {
            print(" ");
        }
    }

    sprintf(buffer, "] %d%%", percentage);

    UART_16550_write_string(UART0, buffer, portMAX_DELAY);

    fflush(stdout);

    if (cur >= max)
    {
        print("\r\nTransfer Complete.\r\n");
    }
}

uint32_t transfer_large_file(const char *pcFileName, uint8_t *destination)
{
    char buffer[50];
    FF_FILE *pxFile;
    size_t xBytesRead;
    unsigned long fileSize = 0, totalBytesRead = 0;

    pxFile = ff_fopen(pcFileName, "rb");
    if (pxFile != NULL)
    {
        sprintf(buffer, "opening %s to %p\r\n", pcFileName, (void *)destination);

        UART_16550_write_string(UART0, buffer, portMAX_DELAY);

        fileSize = pxFile->ulFileSize;
        while ((xBytesRead = ff_fread(destination, 1, CHUNK_SIZE, pxFile)) > 0)
        {
            totalBytesRead += xBytesRead;
            // destination += xBytesRead;
            update_loading_bar(totalBytesRead, fileSize);

            // Safety break: if we read less than CHUNK_SIZE, we are definitely done.
        }

        asm volatile ("fence rw, rw"); // Ensure all writes are completed to memory
        asm volatile ("fence.i");      // Ensure instructions are synchronized

        ff_fclose(pxFile);
    }

    sprintf(buffer, "last cycle wrote %d bytes \r\n", xBytesRead);
    
    UART_16550_write_string(UART0, buffer, portMAX_DELAY);
    
    sprintf(buffer, "wrote %lu bytes\r\n\n", totalBytesRead);

    UART_16550_write_string(UART0, buffer, portMAX_DELAY);

    return fileSize;
}

void load_task(void *pvParameters)
{
    char buffer[50];

    FF_FindData_t xFind;
    FF_Disk_t *pxDisk;
    unsigned long fileSize;
    int rc;

    uint8_t *wad_ptr = (uint8_t *)&__wad_start;   // 0x11000000
    uint8_t *doom_ptr = (uint8_t *)&__doom_start; // 0x13000000

    xSemaphoreTake(sd_semaphore, portMAX_DELAY);

    vTaskDelay(pdMS_TO_TICKS(3000));

    print("Mounting SD Card\r\n");

    pxDisk = mount_fs();
    if (pxDisk == NULL)
    {
        return;
    }

    print("Starting Read from SD Card\r\n");

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

    // transfer_large_file("/sd/doom.wad", wad_ptr);
    transfer_large_file("/sd/program.bin", doom_ptr);

    print("\nJumping to 0x13000000\r\n");

    unmount_disk();

    jump_to_application(doom_ptr);

    // should never happen
    while (1)
    {
    }
}

StaticTask_t load_TCB;

StackType_t load_stack[LOAD_STACK_SIZE];