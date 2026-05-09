// FreeRTOSFATConfig.h -- FreeRTOS+FAT configuration for SD driver
//
// This file is included by the FreeRTOS+FAT source to configure
// the file system library.  See the FreeRTOS+FAT documentation
// for a description of each setting.

#ifndef FREERTOS_FAT_CONFIG_H
#define FREERTOS_FAT_CONFIG_H

// Byte order: ARM Cortex-M is little-endian
#define ffconfigBYTE_ORDER                pdFREERTOS_LITTLE_ENDIAN

// Thread local storage index for per-task current working directory.
// FreeRTOS+FAT uses 3 consecutive slots starting here (CWD, errno,
// FF error).  configNUM_THREAD_LOCAL_STORAGE_POINTERS must be >= 3.
#define ffconfigCWD_THREAD_LOCAL_INDEX    0

// Sector size: SD cards always use 512 bytes
#define ffconfigSECTOR_SIZE               512

// Write support: required for creating files and formatting
#define ffconfigWRITE_SUPPORT             1

// Write through: every write flushes to disk immediately.
// Safer on embedded (no risk of losing cached writes on power loss).
#define ffconfigCACHE_WRITE_THROUGH       1

// Long filename support (FAT32 VFAT entries)
#define ffconfigLFN_SUPPORT               1

// Maximum filename length (including path)
#define ffconfigMAX_FILENAME              128

// FAT12 support: not needed for SDHC cards (always FAT16/FAT32)
#define ffconfigFAT12_SUPPORT             0

// Optimise unaligned access: safe on Cortex-M3
#define ffconfigOPTIMISE_UNALIGNED_ACCESS 1

// Number of cache sectors.  Each costs 512 bytes of RAM.
// 4 sectors (2 KB) matches the SD controller's max transfer size.
#define ffconfigCACHE_SIZE                4

// Time support: disabled (no RTC on this platform).
// Files will have a fixed timestamp.
#define ffconfigTIME_SUPPORT              0

// 64-bit file sizes: not needed for typical embedded use
#define ffconfig64_NUM_SUPPORT            0

// Path cache: speeds up repeated access to the same directory
#define ffconfigPATH_CACHE                1
#define ffconfigPATH_CACHE_DEPTH          5

// Hash cache: speeds up filename lookups
#define ffconfigHASH_CACHE                1
#define ffconfigHASH_CACHE_DEPTH          5

// Use pvPortMalloc / vPortFree for internal allocations
#define ffconfigMALLOC( size )            pvPortMalloc( size )
#define ffconfigFREE( ptr )               vPortFree( ptr )

// mkdir support
#define ffconfigMKDIR_SUPPORT             1

// Remove / rename support
#define ffconfigREMOVE_SUPPORT            1

// Unicode support: disabled (ASCII filenames only)
#define ffconfigUNICODE_UTF8_SUPPORT      0
#define ffconfigUNICODE_UTF16_SUPPORT     0

// Mount retry count
#define ffconfigMOUNT_FIND_FREE           1

// Number of open files supported simultaneously
#define ffconfigMAX_FILE_HANDLES          4

// Locking: FreeRTOS+FAT has its own mutex-based locking.
// ff_locking.c provides the implementation.
#define ffconfigLOCKING_SUPPORT           1

// Debug and asserts
#define ffconfigDEBUG                     0
#define ffconfigFPRINTF_SUPPORT           0
#define ffconfigASSERT( x )               configASSERT( x )

// Device-file support (/dev/ nodes): not needed for SD card
#define ffconfigDEV_SUPPORT               0

#endif // FREERTOS_FAT_CONFIG_H
