// sd_fat_disk.h -- FreeRTOS+FAT disk initialization (public API)
//
// Call sd_fat_disk_init() after the SD driver is running and the
// card is initialised.  It creates the IO manager, mounts the
// first partition, and registers the disk with the virtual file
// system so that ff_fopen("/sd/...") works.
//
// If no valid FAT partition is found, sd_fat_disk_format() can
// be called to create a fresh FAT32 file system.

#ifndef SD_FAT_DISK_H
#define SD_FAT_DISK_H

#include "ff_headers.h"

// Initialise the FreeRTOS+FAT disk structure, create the IO
// manager with a sector cache, and attempt to mount partition 0.
//
// Returns the FF_Disk_t pointer on success, NULL on failure.
// The disk is registered at "/sd/" for ff_fopen() etc.
FF_Disk_t *sd_fat_disk_init(void);

// Format the disk as FAT32.  Call this if sd_fat_disk_init()
// returned non-NULL but the mount failed (no valid partition).
//
// Returns FF_ERR_NONE on success.
FF_Error_t sd_fat_disk_format(FF_Disk_t *pxDisk);

void unmount_disk();


#endif // SD_FAT_DISK_H
