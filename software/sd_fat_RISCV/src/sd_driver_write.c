// sd_driver_write.c -- Two-phase SD write helpers
//
// sd_write_start()    — fills the buffer and issues CMD24 or CMD25
// sd_write_complete() — sends CMD12 if needed
//
// Called by the driver task event loop.  Between the two calls the
// event loop waits for XFER_COMPLETE or an error event.

#include <stdint.h>
#include "sd_driver_private.h"

// -----------------------------------------------------------------------
// Phase 1: fill the data buffer and issue the write command
//
// count == 1  → CMD24 WRITE_BLOCK
// count >  1  → CMD25 WRITE_MULTIPLE_BLOCK
// -----------------------------------------------------------------------
void sd_write_start(uint32_t sector, uint32_t count,
                    const uint32_t *src)
{
    int groups;
    int i;

    if (count == 1) {
        REG32(SD_BLOCK_SIZE) = 0x00010200;

        for (i = 0; i < SD_WORDS_PER_SECTOR; i++) {
            REG32(SD_BUFFER_PORT) = src[i];
        }

        // CMD24: data_present | CRC | index | R1 = 0x183A
        //   tm: 0x0000 (single block, write direction)
        REG32(SD_ARGUMENT)      = sd_hw_block_arg(sector);
        REG32(SD_TRANSFER_MODE) = 0x183A0000;
    } else {
        REG32(SD_BLOCK_SIZE) = ((uint32_t)count << 16) | 0x0200;

        groups = (count * SD_WORDS_PER_SECTOR) / 8;
        sd_hw_burst_write((volatile uint32_t *)SD_BUFFER_PORT,
                          src, groups);

        // CMD25: data_present | CRC | index | R1 = 0x193A
        //   tm: multi(bit5) | blk_cnt_en(bit1) = 0x0022
        REG32(SD_ARGUMENT)      = sd_hw_block_arg(sector);
        REG32(SD_TRANSFER_MODE) = 0x193A0022;
    }
}

// -----------------------------------------------------------------------
// Phase 2: stop if multi-block
//
// Called after XFER_COMPLETE arrives.
// -----------------------------------------------------------------------
void sd_write_complete(uint32_t count)
{
    if (count > 1) {
        sd_hw_stop_transmission(sd_event_queue);
    }
}

// -----------------------------------------------------------------------
// SDMA + Auto CMD12 write
// -----------------------------------------------------------------------

// Phase 1: issue CMD24/CMD25 with DMA_ENABLE and Auto CMD12.
// buf_addr is the physical RAM address where SDMA will read data from.
void sd_write_start_dma(uint32_t sector, uint32_t count,
                        uint32_t buf_addr)
{
    uint16_t tm;

    // Write SDMA System Address
    REG32(SD_SDMA_ADDR) = buf_addr;

    if (count == 1) {
        // CMD24: single-block DMA write
        //   tm: DMA_ENABLE(bit0) = 0x0001
        REG32(SD_BLOCK_SIZE) = 0x00010200;
        tm = 0x0001;
        REG32(SD_ARGUMENT)      = sd_hw_block_arg(sector);
        REG32(SD_TRANSFER_MODE) = ((uint32_t)0x183A << 16) | tm;
    } else {
        // CMD25: multi-block DMA write with Auto CMD12
        //   tm: DMA_ENABLE(bit0) | blk_cnt_en(bit1) | auto_cmd12(bit2)
        //       | multi(bit5) = 0x0027
        REG32(SD_BLOCK_SIZE) = ((uint32_t)count << 16) | 0x0200;
        tm = 0x0027;
        REG32(SD_ARGUMENT)      = sd_hw_block_arg(sector);
        REG32(SD_TRANSFER_MODE) = ((uint32_t)0x193A << 16) | tm;
    }
}

// Phase 2: DMA write complete — Auto CMD12 has already stopped
// the transfer for multi-block.
void sd_write_complete_dma(uint32_t count)
{
    if (count > 1) {
        sd_hw_wait_dat_release();
    }
}
