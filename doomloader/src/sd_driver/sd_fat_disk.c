// sd_fat_disk.c -- FreeRTOS+FAT disk initialization
//
// Creates the FF_Disk_t, allocates the sector cache, creates the
// IO manager, mounts the first partition, and registers it with
// the FreeRTOS+FAT virtual file system.

#include <stdint.h>
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <ff_headers.h>
#include "ff_sys.h"
#include "ff_stdio.h"
#include "sd_driver.h"
#include <sd_fat_disk.h>

// -----------------------------------------------------------------------
// Static storage
// -----------------------------------------------------------------------

// Sector cache: ffconfigCACHE_SIZE sectors × 512 bytes each
#define SD_CACHE_SIZE  (ffconfigCACHE_SIZE * ffconfigSECTOR_SIZE)
static uint8_t ucCacheMemory[SD_CACHE_SIZE];

// FF_Disk_t — the disk descriptor
static FF_Disk_t xDisk;

// Mount point for the virtual file system
#define SD_MOUNT_POINT  "/sd"

// -----------------------------------------------------------------------
// Debug print (reuse UART from driver)
// -----------------------------------------------------------------------
extern void sd_dbg_print(const char *s);
extern void sd_dbg_printf(const char *fmt_str, ...);

// -----------------------------------------------------------------------
// Public: initialise the disk and attempt to mount
// -----------------------------------------------------------------------
FF_Disk_t *sd_fat_disk_init(void)
{
    const sd_card_info_t *info;
    FF_CreationParameters_t xParams;
    FF_Error_t xError;

    info = sd_driver_get_card_info();
    if (!info->initialised) {
        sd_dbg_print("  fat: card not initialised\r\n");
        return NULL;
    }
    if (info->total_sectors == 0) {
        sd_dbg_print("  fat: card capacity unknown\r\n");
        return NULL;
    }

    sd_dbg_printf("  fat: card has %lu sectors (%lu MB)\r\n",
                  (unsigned long)info->total_sectors,
                  (unsigned long)(info->total_sectors / 2048));

    // Zero the disk descriptor
    memset(&xDisk, 0, sizeof(xDisk));

    // Fill in creation parameters
    memset(&xParams, 0, sizeof(xParams));
    xParams.pucCacheMemory = ucCacheMemory;
    xParams.ulMemorySize   = sizeof(ucCacheMemory);
    xParams.ulSectorSize   = ffconfigSECTOR_SIZE;
    xParams.fnWriteBlocks  = sd_ff_write_blocks;
    xParams.fnReadBlocks   = sd_ff_read_blocks;
    xParams.pxDisk         = &xDisk;
    xParams.pvSemaphore    = xSemaphoreCreateRecursiveMutex();
    if (xParams.pvSemaphore == NULL) {
        sd_dbg_print("  fat: failed to create IO manager semaphore\r\n");
        return NULL;
    }

    // Create the IO manager (allocates internal structures)
    xDisk.pxIOManager = FF_CreateIOManger(&xParams, &xError);
    if (xDisk.pxIOManager == NULL) {
        sd_dbg_printf("  fat: FF_CreateIOManger failed (0x%lX)\r\n",
                      (unsigned long)xError);
        return NULL;
    }

    // Set disk geometry
    xDisk.ulNumberOfSectors = info->total_sectors;
    xDisk.xStatus.bIsInitialised = pdTRUE;

    // Attempt to mount partition 0
    xError = FF_Mount(&xDisk, 0);
    if (FF_isERR(xError)) {
        sd_dbg_printf("  fat: mount failed (0x%lX) — format needed\r\n",
                      (unsigned long)xError);
        // Return the disk anyway so the caller can format it.
        // bPartitionNumber stays 0 (unset), ulTotalSectors stays 0
        // in the partition struct, signalling "not mounted".
        FF_FS_Add(SD_MOUNT_POINT, &xDisk);
        return &xDisk;
    }

    sd_dbg_print("  fat: partition 0 mounted\r\n");
    xDisk.xStatus.bPartitionNumber = 0;

    // Register with the virtual file system
    FF_FS_Add(SD_MOUNT_POINT, &xDisk);
    sd_dbg_printf("  fat: registered at %s\r\n", SD_MOUNT_POINT);

    return &xDisk;
}

// -----------------------------------------------------------------------
// Public: format the disk as FAT32
// -----------------------------------------------------------------------
FF_Error_t sd_fat_disk_format(FF_Disk_t *pxDisk)
{
    FF_Error_t xError;
    FF_PartitionParameters_t xPartParams;

    sd_dbg_print("  fat: creating partition table...\r\n");

    // Create an MBR with a single primary partition using all space
    memset(&xPartParams, 0, sizeof(xPartParams));
    xPartParams.ulSectorCount  = pxDisk->ulNumberOfSectors;
    xPartParams.ulHiddenSectors = 0;
    xPartParams.ulInterSpace   = 0;
    xPartParams.xSizes[0]      = 100;  // 100% of disk
    xPartParams.xPrimaryCount  = 1;
    xPartParams.eSizeType      = eSizeIsPercent;

    xError = FF_Partition(pxDisk, &xPartParams);
    if (FF_isERR(xError)) {
        sd_dbg_printf("  fat: partition failed (0x%lX)\r\n",
                      (unsigned long)xError);
        return xError;
    }

    sd_dbg_print("  fat: formatting as FAT32...\r\n");

    // FF_Format parameters:
    //   pxDisk:      disk to format
    //   xPartition:  partition number (0)
    //   xPreferFAT:  pdFALSE = auto-select FAT type based on size
    //   xSmallClusters: pdFALSE = standard cluster sizing
    xError = FF_Format(pxDisk, 0, pdFALSE, pdFALSE);
    if (FF_isERR(xError)) {
        sd_dbg_printf("  fat: format failed (0x%lX)\r\n",
                      (unsigned long)xError);
        return xError;
    }

    sd_dbg_print("  fat: format complete\r\n");

    // Mount the freshly formatted partition
    xError = FF_Mount(pxDisk, 0);
    if (FF_isERR(xError)) {
        sd_dbg_printf("  fat: mount after format failed (0x%lX)\r\n",
                      (unsigned long)xError);
        return xError;
    }

    sd_dbg_print("  fat: mounted after format\r\n");
    pxDisk->xStatus.bPartitionNumber = 0;
    return FF_ERR_NONE;
}
