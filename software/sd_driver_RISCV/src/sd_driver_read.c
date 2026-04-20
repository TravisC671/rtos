// sd_driver_read.c -- Two-phase SD read helpers
//
// sd_read_start()    — issues CMD17 or CMD18
// sd_read_complete() — burst-reads the buffer and sends CMD12 if needed
//
// Called by the driver task event loop.  Between the two calls the
// event loop waits for XFER_COMPLETE or an error event.

#include <stdint.h>
#include "sd_driver_private.h"

// -----------------------------------------------------------------------
// Phase 1: issue the read command
//
// count == 1  → CMD17 READ_SINGLE_BLOCK
// count >  1  → CMD18 READ_MULTIPLE_BLOCK
// -----------------------------------------------------------------------
void sd_read_start(uint32_t sector, uint32_t count)
{
    if (count == 1) {
        // CMD17: data_present | CRC | index | R1 = 0x113A
        //   tm: dir_read(bit4) = 0x0010
        REG32(SD_BLOCK_SIZE) = 0x00010200;
        REG32(SD_ARGUMENT)      = sd_hw_block_arg(sector);
        REG32(SD_TRANSFER_MODE) = 0x113A0010;
    } else {
        // CMD18: data_present | CRC | index | R1 = 0x123A
        //   tm: multi(bit5) | dir_read(bit4) | blk_cnt_en(bit1) = 0x0032
        REG32(SD_BLOCK_SIZE) = ((uint32_t)count << 16) | 0x0200;
        REG32(SD_ARGUMENT)      = sd_hw_block_arg(sector);
        REG32(SD_TRANSFER_MODE) = 0x123A0032;
    }
}

// -----------------------------------------------------------------------
// Phase 2: drain the buffer and stop if multi-block
//
// Called after XFER_COMPLETE arrives.
// -----------------------------------------------------------------------
void sd_read_complete(uint32_t *dst, uint32_t count)
{
    int groups;

    groups = (count * SD_WORDS_PER_SECTOR) / 8;
    sd_hw_burst_read((const volatile uint32_t *)SD_BUFFER_PORT,
                     dst, groups);

    if (count > 1) {
        sd_hw_stop_transmission(sd_event_queue);
    }
}

// -----------------------------------------------------------------------
// SDMA + Auto CMD12 read
// -----------------------------------------------------------------------

// Phase 1: issue CMD17/CMD18 with DMA_ENABLE and Auto CMD12.
// buf_addr is the physical RAM address where SDMA will write data.
void sd_read_start_dma(uint32_t sector, uint32_t count,
                       uint32_t buf_addr)
{
    uint16_t tm;

    // Write SDMA System Address
    REG32(SD_SDMA_ADDR) = buf_addr;

    if (count == 1) {
        // CMD17: single-block DMA read
        //   tm: DMA_ENABLE(bit0) | dir_read(bit4) = 0x0011
        REG32(SD_BLOCK_SIZE) = 0x00010200;
        tm = 0x0011;
        REG32(SD_ARGUMENT)      = sd_hw_block_arg(sector);
        REG32(SD_TRANSFER_MODE) = ((uint32_t)0x113A << 16) | tm;
    } else {
        // CMD18: multi-block DMA read with Auto CMD12
        //   tm: DMA_ENABLE(bit0) | blk_cnt_en(bit1) | auto_cmd12(bit2)
        //       | dir_read(bit4) | multi(bit5) = 0x0037
        REG32(SD_BLOCK_SIZE) = ((uint32_t)count << 16) | 0x0200;
        tm = 0x0037;
        REG32(SD_ARGUMENT)      = sd_hw_block_arg(sector);
        REG32(SD_TRANSFER_MODE) = ((uint32_t)0x123A << 16) | tm;
    }
}

// Phase 2: DMA read complete — data is already in RAM via SDMA,
// and Auto CMD12 has already stopped the transfer for multi-block.
void sd_read_complete_dma(uint32_t count)
{
    if (count > 1) {
        sd_hw_wait_dat_release();
    }
}
