// sd_driver.h -- FreeRTOS+FAT SD card driver (public API)
//
// Architecture:
//
//   FF_ReadBlocks / FF_WriteBlocks
//       |  post args to argument queue
//       |  post READ_REQ or WRITE_REQ to event queue
//       |  block on task notification until complete
//       v
//   ┌──────────────────────────────────────────┐
//   │  Unified Event Queue                      │
//   │   <- ISR posts SD_EVT_INT                 │
//   │   <- FF_Read posts SD_EVT_READ_REQ        │
//   │   <- FF_Write posts SD_EVT_WRITE_REQ      │
//   └──────────────┬───────────────────────────┘
//                  v
//   ┌──────────────────────────────────────────┐
//   │  SD Driver Task                           │
//   │   - card init / power cycle               │
//   │   - calls sd_do_read / sd_do_write        │
//   │     directly (waits on event queue for    │
//   │     interrupt events during IO)           │
//   │   - handles card insert / remove          │
//   └──────────────────────────────────────────┘

#ifndef SD_DRIVER_H
#define SD_DRIVER_H

#include <stdint.h>

// -----------------------------------------------------------------------
// DMA region table
//
// The SDMA AXI master can only access memory on the shared AXI bus
// (e.g. DDR2 at 0x80000000).  Internal BRAM (0x20000000) is on the
// CPU's private AHB bus and is NOT DMA-accessible.
//
// Register DMA-capable regions BEFORE calling sd_driver_init().
// The driver automatically uses SDMA for buffers within a registered
// region and falls back to PIO for all other addresses.
// -----------------------------------------------------------------------
#define SD_MAX_DMA_REGIONS  4

// Register a DMA-capable memory region.
// Returns 0 on success, -1 if the table is full.
int sd_driver_add_dma_region(uint32_t base, uint32_t size);

// -----------------------------------------------------------------------
// Initialisation
// -----------------------------------------------------------------------

// Create all driver tasks and queues, initialise the SD controller
// hardware, power-cycle the card, and run the SD init sequence
// (CMD0 .. CMD7 + ACMD6).
//
// Call once from main() before starting the scheduler.
// Returns 1 on success, 0 on failure.
int sd_driver_init(void);

// -----------------------------------------------------------------------
// Block-level IO  (callable from any task context)
// -----------------------------------------------------------------------

// Read 'count' 512-byte sectors starting at 'sector' into 'buffer'.
// Blocks the calling task until the transfer completes.
// Returns 0 on success, negative on error.
int32_t sd_driver_read_blocks(uint8_t *buffer,
                              uint32_t sector,
                              uint32_t count);

// Write 'count' 512-byte sectors starting at 'sector' from 'buffer'.
// Blocks the calling task until the transfer completes.
// Returns 0 on success, negative on error.
int32_t sd_driver_write_blocks(const uint8_t *buffer,
                               uint32_t sector,
                               uint32_t count);

// -----------------------------------------------------------------------
// Card information
// -----------------------------------------------------------------------
typedef struct {
    uint16_t rca;           // Relative Card Address
    int      is_sdhc;       // 1 = SDHC/SDXC (block addressing)
    int      initialised;   // 1 = card init succeeded
    uint32_t total_sectors; // card capacity in 512-byte sectors
} sd_card_info_t;

const sd_card_info_t *sd_driver_get_card_info(void);

// -----------------------------------------------------------------------
// FreeRTOS+FAT integration
//
// These match the FF_ReadBlocks_t / FF_WriteBlocks_t signatures
// expected by FreeRTOS-Plus-FAT.  Pass them as callbacks when
// creating the FF_Disk_t.
// -----------------------------------------------------------------------
struct xFFDisk;  // forward declaration

int32_t sd_ff_read_blocks(uint8_t *pucBuffer,
                          uint32_t ulSectorAddress,
                          uint32_t ulCount,
                          struct xFFDisk *pxDisk);

int32_t sd_ff_write_blocks(uint8_t *pucBuffer,
                           uint32_t ulSectorAddress,
                           uint32_t ulCount,
                           struct xFFDisk *pxDisk);

#endif // SD_DRIVER_H
