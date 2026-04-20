// sd_driver_task.c -- SD Driver task and initialisation
//
// This file contains:
//   - All queue definitions (storage owners)
//   - The SD card software initialisation sequence
//   - The driver task main loop (event dispatch)
//   - sd_driver_init() which creates everything
//
// Read and write operations are performed as function calls within
// the driver task context, using sd_event_queue directly for
// interrupt events.  There are no separate read/write tasks.

#include <stdint.h>
#include <string.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <UART_16550.h>
#include "sd_driver.h"
#include "sd_driver_private.h"

// Cache management for DMA coherency (MicroBlaze V dcache is always on)
extern void riscv_flush_dcache_range(unsigned int addr, unsigned int len);
extern void riscv_invalidate_dcache_range(unsigned int addr, unsigned int len);

// -----------------------------------------------------------------------
// Queue handles  (declared extern in sd_driver_private.h)
// -----------------------------------------------------------------------
QueueHandle_t sd_event_queue;
QueueHandle_t sd_arg_queue;

// -----------------------------------------------------------------------
// Static storage for queues (no dynamic allocation)
// -----------------------------------------------------------------------
static StaticQueue_t  event_q_buf;
static uint8_t        event_q_store[SD_EVENT_QUEUE_LEN];

static StaticQueue_t  arg_q_buf;
static sd_io_request_t arg_q_store[SD_ARG_QUEUE_LEN];

// -----------------------------------------------------------------------
// Static storage for the driver task
// -----------------------------------------------------------------------
static StaticTask_t driver_tcb;
static StackType_t  driver_stack[SD_DRIVER_STACK_SIZE];

// -----------------------------------------------------------------------
// Shared card state
// -----------------------------------------------------------------------
sd_card_state_t sd_card;

// -----------------------------------------------------------------------
// DMA region table
// -----------------------------------------------------------------------
static struct { uint32_t base; uint32_t size; } dma_regions[SD_MAX_DMA_REGIONS];
static int dma_region_count;

int sd_driver_add_dma_region(uint32_t base, uint32_t size)
{
    if (dma_region_count >= SD_MAX_DMA_REGIONS)
        return -1;
    dma_regions[dma_region_count].base = base;
    dma_regions[dma_region_count].size = size;
    dma_region_count++;
    return 0;
}

// Check whether the entire buffer [buf, buf+byte_count) falls within
// a registered DMA-capable region.
static int addr_is_dma_capable(const uint8_t *buf, uint32_t byte_count)
{
    uint32_t start = (uint32_t)buf;
    uint32_t end   = start + byte_count;
    int i;

    for (i = 0; i < dma_region_count; i++) {
        if (start >= dma_regions[i].base &&
            end   <= dma_regions[i].base + dma_regions[i].size)
            return 1;
    }
    return 0;
}

// -----------------------------------------------------------------------
// SD card software initialisation
//
// CMD0 → CMD8 → CMD55/ACMD41 loop → CMD2 → CMD3 → CMD7 → ACMD6
//
// The driver task receives INT events directly from sd_event_queue.
// -----------------------------------------------------------------------
static int sd_card_init(void)
{
    uint32_t r0;
    int ready = 0;
    uint32_t acmd41_arg = 0x40FF8000;  // HCS=1, voltage 3.2-3.6V
    int retry;
    uint32_t ocr;
    uint32_t resp1;
    uint32_t resp2;
    uint32_t c_size;
    uint32_t c_size_mult;
    uint32_t read_bl_len;
    uint32_t csd_struct;

    // sd_dbg_print("  sd_drv: starting card init\r\n");

    // -- CMD0: GO_IDLE_STATE (no response) -- send twice
    sd_hw_send_cmd(sd_hw_make_cmd(0, 0, 0, 0),
                   0x00000000, 0x0000, 100, sd_event_queue);
    vTaskDelay(pdMS_TO_TICKS(10));
    if (!sd_hw_send_cmd(sd_hw_make_cmd(0, 0, 0, 0),
                        0x00000000, 0x0000, 100, sd_event_queue))
    {
        sd_dbg_print("  sd_drv: CMD0 failed\r\n");
        return 0;
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    // -- CMD8: SEND_IF_COND (R7, CRC, index) --
    // sd_dbg_print("  sd_drv: CMD8 SEND_IF_COND\r\n");
    if (!sd_hw_send_cmd(sd_hw_make_cmd(8, 2, 1, 1),
                        0x000001AA, 0x0000, 100, sd_event_queue))
    {
        sd_dbg_print("  sd_drv: CMD8 failed (SD v1.x?)\r\n");
    } else {
        r0 = REG32(SD_RESPONSE0);
        if ((r0 & 0x1FF) != 0x1AA) {
            sd_dbg_printf("  sd_drv: CMD8 echo mismatch 0x%08lX\r\n",
                          (unsigned long)r0);
            return 0;
        }
    }

    // -- CMD55 + ACMD41 loop (poll until OCR ready) --
    // sd_dbg_print("  sd_drv: ACMD41 polling\r\n");

    for (retry = 0; retry < 50; retry++) {
        if (!sd_hw_send_cmd(sd_hw_make_cmd(55, 2, 1, 1),
                            0x00000000, 0x0000, 100, sd_event_queue))
        {
            sd_dbg_print("  sd_drv: CMD55 failed\r\n");
            return 0;
        }
        vTaskDelay(pdMS_TO_TICKS(1));  // Ncc + yield
        if (!sd_hw_send_cmd(sd_hw_make_cmd(41, 2, 0, 0),
                            acmd41_arg, 0x0000, 100, sd_event_queue))
        {
            sd_dbg_print("  sd_drv: ACMD41 failed\r\n");
            return 0;
        }

        ocr = REG32(SD_RESPONSE0);
        if (ocr & (1u << 31)) {
            sd_card.is_sdhc = (ocr >> 30) & 1;
            // sd_dbg_printf("  sd_drv: OCR ready, %s\r\n",
            //               sd_card.is_sdhc ? "SDHC" : "SDSC");
            ready = 1;
            break;
        }

        // Fallback: try without HCS after 25 retries
        if (retry == 25 && acmd41_arg != 0x00FF8000) {
            acmd41_arg = 0x00FF8000;
            sd_dbg_print("  sd_drv: ACMD41 fallback (no HCS)\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (!ready) {
        sd_dbg_print("  sd_drv: ACMD41 timeout\r\n");
        return 0;
    }

    // -- CMD2: ALL_SEND_CID (R2/136, CRC, no index) --
    // sd_dbg_print("  sd_drv: CMD2 ALL_SEND_CID\r\n");
    if (!sd_hw_send_cmd(sd_hw_make_cmd(2, 1, 1, 0),
                        0x00000000, 0x0000, 100, sd_event_queue))
    {
        sd_dbg_print("  sd_drv: CMD2 failed\r\n");
        return 0;
    }

    // -- CMD3: SEND_RELATIVE_ADDR (R6, CRC, index) --
    // sd_dbg_print("  sd_drv: CMD3 SEND_RELATIVE_ADDR\r\n");
    if (!sd_hw_send_cmd(sd_hw_make_cmd(3, 2, 1, 1),
                        0x00000000, 0x0000, 100, sd_event_queue))
    {
        sd_dbg_print("  sd_drv: CMD3 failed\r\n");
        return 0;
    }
    sd_card.rca = (uint16_t)(REG32(SD_RESPONSE0) >> 16);
    // sd_dbg_printf("  sd_drv: RCA=0x%04X\r\n", sd_card.rca);

    // -- CMD9: SEND_CSD (R2/136, CRC, no index) --
    // Must be sent while card is in stand-by state (before CMD7).
    // The 128-bit CSD register is mapped across RESPONSE3..RESPONSE0.
    // sd_dbg_print("  sd_drv: CMD9 SEND_CSD\r\n");
    if (!sd_hw_send_cmd(sd_hw_make_cmd(9, 1, 1, 0),
                        (uint32_t)sd_card.rca << 16, 0x0000,
                        100, sd_event_queue))
    {
        sd_dbg_print("  sd_drv: CMD9 failed, capacity unknown\r\n");
        sd_card.total_sectors = 0;
    } else {
        // SDHCI response mapping for R2:
        //   RESPONSE3[23:0]  = CSD[127:104]
        //   RESPONSE2[31:0]  = CSD[103:72]
        //   RESPONSE1[31:0]  = CSD[71:40]
        //   RESPONSE0[31:0]  = CSD[39:8]
        resp2 = REG32(SD_RESPONSE2);
        resp1 = REG32(SD_RESPONSE1);
        csd_struct = (REG32(SD_RESPONSE3) >> 22) & 0x03;

        if (csd_struct == 1) {
            // CSD Version 2.0 (SDHC/SDXC)
            // C_SIZE = CSD[69:48] → RESPONSE1 bits [29:8]
            c_size = (resp1 >> 8) & 0x3FFFFF;
            sd_card.total_sectors = (c_size + 1) * 1024;
        } else {
            // CSD Version 1.0 (SDSC)
            // READ_BL_LEN = CSD[83:80] → RESPONSE2 bits [11:8]
            // C_SIZE      = CSD[73:62] → RESPONSE2[1:0] | RESPONSE1[31:22]
            // C_SIZE_MULT = CSD[49:47] → RESPONSE1 bits [9:7]
            read_bl_len = (resp2 >> 8) & 0x0F;
            c_size = ((resp2 & 0x03) << 10) | ((resp1 >> 22) & 0x3FF);
            c_size_mult = (resp1 >> 7) & 0x07;
            sd_card.total_sectors =
                (uint32_t)(c_size + 1) * (1u << (c_size_mult + 2))
                * (1u << read_bl_len) / 512;
        }
        // sd_dbg_printf("  sd_drv: %lu sectors (%lu MB)\r\n",
        //               (unsigned long)sd_card.total_sectors,
        //               (unsigned long)(sd_card.total_sectors / 2048));
    }

    // -- CMD7: SELECT_CARD (R1b, CRC, index) --
    // sd_dbg_print("  sd_drv: CMD7 SELECT_CARD\r\n");
    if (!sd_hw_send_cmd(sd_hw_make_cmd(7, 3, 1, 1),
                        (uint32_t)sd_card.rca << 16, 0x0000,
                        500, sd_event_queue))
    {
        sd_dbg_print("  sd_drv: CMD7 failed\r\n");
        return 0;
    }

    // -- CMD55 + ACMD6: SET_BUS_WIDTH (4-bit) --
    // Non-fatal: if this fails we continue in 1-bit mode.
    // sd_dbg_print("  sd_drv: ACMD6 SET_BUS_WIDTH\r\n");
    if (!sd_hw_send_cmd(sd_hw_make_cmd(55, 2, 1, 1),
                        (uint32_t)sd_card.rca << 16, 0x0000,
                        100, sd_event_queue) ||
        !sd_hw_send_cmd(sd_hw_make_cmd(6, 2, 1, 1),
                        0x00000002, 0x0000, 100, sd_event_queue))
    {
        sd_dbg_print("  sd_drv: ACMD6 failed, continuing in 1-bit mode\r\n");
        REG8(SD_HOST_CTRL1) = REG8(SD_HOST_CTRL1) & ~0x02;
    } else {
        // Card is now in 4-bit mode; tell the host controller to match
        REG8(SD_HOST_CTRL1) = REG8(SD_HOST_CTRL1) | 0x02;
    }

    sd_card.initialised = 1;
    // sd_dbg_print("  sd_drv: card init OK\r\n");
    return 1;
}

// -----------------------------------------------------------------------
// Try to switch the card to High Speed mode via CMD6 SWITCH_FUNCTION
//
// CMD6 returns 512 bits (64 bytes) on the DAT lines.  We set
// block_size=64, block_count=1, and issue CMD6 with data_present +
// read direction.  The switch status bytes tell us whether the card
// supports and accepted High Speed (function group 1, function 1).
//
// Returns 1 if High Speed mode was activated, 0 otherwise.
// -----------------------------------------------------------------------
static int sd_try_high_speed(void)
{
    uint16_t norm;
    uint32_t buf[16];    // 64 bytes = 16 words
    uint16_t func_supported;
    uint8_t  func_selected;
    int i;

    // sd_dbg_print("  sd_drv: CMD6 SWITCH_FUNCTION (High Speed)\r\n");

    sd_hw_clear_interrupts(sd_event_queue);

    // Block size = 64, block count = 1
    REG32(SD_BLOCK_SIZE) = 0x00010040;

    // CMD6: SWITCH_FUNCTION
    //   index=6, resp=R1(2), CRC check, index check, data_present(bit5)
    //   cmd_reg = 0x063A, tm = 0x0010 (read direction)
    //   arg = 0x80FFFFF1: switch mode, access mode = High Speed
    REG32(SD_ARGUMENT)      = 0x80FFFFF1;
    REG32(SD_TRANSFER_MODE) = 0x063A0010;

    // Wait for transfer complete or error
    norm = sd_hw_wait_int(
        SD_INT_XFER_COMPLETE | SD_INT_ERROR, 2000,
        sd_event_queue);

    if (!(norm & SD_INT_XFER_COMPLETE) || (norm & SD_INT_ERROR)) {
        sd_dbg_print("  sd_drv: CMD6 failed or timed out\r\n");
        REG8(SD_SW_RESET) = SD_RESET_DAT;
        (void)REG32(SD_CLOCK_CTRL);
        sd_hw_clear_interrupts(sd_event_queue);
        // Restore block size to 512 for normal transfers
        REG32(SD_BLOCK_SIZE) = 0x00010200;
        return 0;
    }

    // Read 16 words (64 bytes) from buffer
    for (i = 0; i < 16; i++) {
        buf[i] = REG32(SD_BUFFER_PORT);
    }

    sd_hw_clear_interrupts(sd_event_queue);

    // Restore block size to 512 for normal transfers
    REG32(SD_BLOCK_SIZE) = 0x00010200;

    // Parse switch status (SD Physical Layer Spec, Table 4-11):
    // The 512-bit status is big-endian on the wire.  Each 32-bit word
    // read from the buffer data port is in little-endian byte order
    // but the words appear in big-endian word order (word 0 contains
    // the highest-numbered bits).
    //
    // Function group 1 supported functions: bits [415:400]
    //   = word index 3 (bits 415:384), bits [31:16] of that word
    // Function group 1 selection result: bits [379:376]
    //   = word index 4 (bits 383:352), bits [27:24] of that word
    //   (the low nibble of byte 16, which is bits 379:376)

    func_supported = (uint16_t)(((buf[3] & 0xFF) << 8) | ((buf[3] >> 8) & 0xFF));
    func_selected  = (uint8_t)(buf[4] & 0x0F);

    // sd_dbg_printf("  sd_drv: CMD6 group1 supported=0x%04X selected=%d\r\n",
    //               func_supported, func_selected);

    if (func_selected == 1) {
        // sd_dbg_print("  sd_drv: High Speed mode activated\r\n");
        return 1;
    }

    // sd_dbg_print("  sd_drv: High Speed not available\r\n");
    return 0;
}

// -----------------------------------------------------------------------
// Hardware setup: power-cycle card, configure clocks and enables
// -----------------------------------------------------------------------
static void sd_hw_setup(void)
{
    int wait;
    uint32_t ps;
    int i;

    // Power-cycle the card via Power Control register
    REG8(SD_POWER_CTRL) = 0x0E;  // VDD off, keep 3.3V select
    vTaskDelay(pdMS_TO_TICKS(100));       // discharge
    REG8(SD_POWER_CTRL) = 0x0F;  // VDD on

    // Full controller reset
    REG8(SD_SW_RESET) = SD_RESET_ALL;
    vTaskDelay(pdMS_TO_TICKS(10));

    // SW_RESET_ALL clears ALL registers including Power Control.
    // Re-enable VDD so the card stays powered through initialization.
    REG8(SD_POWER_CTRL) = 0x0F;

    // Data timeout: maximum (2^27 cycles)
    REG8(SD_TIMEOUT_CTRL) = 0x0E;

    // SD clock 400 kHz for init (100 MHz / (2 × 125) = 400 kHz)
    REG16(SD_CLOCK_CTRL) = 0x7D01;       // freq=125, int_clk_en
    wait = 0;
    while (!(REG16(SD_CLOCK_CTRL) & 0x0002)) {
        vTaskDelay(pdMS_TO_TICKS(1));
        if (++wait > 100) break;
    }
    REG16(SD_CLOCK_CTRL) = 0x7D05;       // + sd_clk_en

    // Enable interrupt status + signal enables
    // Normal: CMD_COMPLETE(0), XFER_COMPLETE(1), DMA_INT(3),
    //         CARD_INSERT(6), CARD_REMOVE(7)
    // Skip BUF_WRITE_READY(4) and BUF_READ_READY(5) — hardware asserts these
    // whenever the buffer is idle, causing an interrupt storm.
    REG16(SD_NORM_INT_STAT_EN) = 0x00CB;
    REG16(SD_ERR_INT_STAT_EN)  = 0x01FF;
    REG16(SD_NORM_INT_SIG_EN)  = 0x00CB;
    REG16(SD_ERR_INT_SIG_EN)   = 0x01FF;

    // Wait for card power-up after VDD restoration
    vTaskDelay(pdMS_TO_TICKS(250));

    // Verify card is inserted
    ps = REG32(SD_PRESENT_STATE);
    if (!(ps & SD_STATE_CARD_INSERTED)) {
        // sd_dbg_print("  sd_drv: waiting for card...\r\n");
        for (i = 0; i < 100; i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
            ps = REG32(SD_PRESENT_STATE);
            if ((ps & SD_STATE_CARD_INSERTED) &&
                (ps & SD_STATE_CARD_STABLE))
                break;
        }
    }
    sd_hw_clear_interrupts(sd_event_queue);
}

// -----------------------------------------------------------------------
// Driver task state machine
// -----------------------------------------------------------------------
typedef enum {
    DRV_IDLE,
    DRV_READ_WAIT,
    DRV_WRITE_WAIT,
    DRV_DMA_READ_WAIT,
    DRV_DMA_WRITE_WAIT
} drv_state_t;

// Operation context (valid when drv_state != DRV_IDLE)
static drv_state_t       drv_state;
static uint8_t          *op_buf;
static uint32_t          op_sector;
static uint32_t          op_remaining;
static uint32_t          op_chunk;
static TaskHandle_t      op_caller;
static volatile int32_t *op_p_result;

// Start the next PIO read chunk (max 4 sectors per buffer)
static void start_read_chunk(void)
{
    sd_hw_clear_interrupts(sd_event_queue);
    op_chunk = (op_remaining > SD_MAX_SECTORS_PER_XFER)
                ? SD_MAX_SECTORS_PER_XFER : op_remaining;
    sd_read_start(op_sector, op_chunk);
}

// Start the next PIO write chunk (max 4 sectors per buffer)
static void start_write_chunk(void)
{
    sd_hw_clear_interrupts(sd_event_queue);
    op_chunk = (op_remaining > SD_MAX_SECTORS_PER_XFER)
                ? SD_MAX_SECTORS_PER_XFER : op_remaining;
    sd_write_start(op_sector, op_chunk, (const uint32_t *)op_buf);
}

// Start the next DMA read (no chunk size limit — SDMA handles boundaries)
static void start_dma_read(void)
{
    sd_hw_clear_interrupts(sd_event_queue);
    op_chunk = op_remaining;
    sd_read_start_dma(op_sector, op_chunk, (uint32_t)op_buf);
}

// Start the next DMA write (no chunk size limit — SDMA handles boundaries)
static void start_dma_write(void)
{
    sd_hw_clear_interrupts(sd_event_queue);
    op_chunk = op_remaining;
    // Flush dcache so SDMA reads correct data from DDR
    riscv_flush_dcache_range((unsigned int)op_buf,
                             op_chunk * SD_SECTOR_SIZE);
    sd_write_start_dma(op_sector, op_chunk, (uint32_t)op_buf);
}

// Complete the current operation and notify the caller
static void finish_op(int32_t result)
{
    *op_p_result = result;
    xTaskNotifyGive(op_caller);
    drv_state = DRV_IDLE;
}

// Abort the current operation on error
static void abort_op(void)
{
    REG8(SD_SW_RESET) = SD_RESET_DAT;
    (void)REG32(SD_CLOCK_CTRL);
    sd_hw_clear_interrupts(sd_event_queue);
    finish_op(-1);
}

// Advance buffer/sector/remaining after a successful chunk
static void advance_chunk(void)
{
    op_buf       += op_chunk * SD_SECTOR_SIZE;
    op_sector    += op_chunk;
    op_remaining -= op_chunk;
}

// -----------------------------------------------------------------------
// Event handlers for each state
// -----------------------------------------------------------------------

static void handle_idle(uint8_t evt)
{
    sd_io_request_t req;

    switch (evt) {

    case SD_EVT_READ_REQ:
        if (xQueueReceive(sd_arg_queue, &req, 0) != pdTRUE) {
            sd_dbg_print("  sd_drv: READ_REQ but no args!\r\n");
            break;
        }
        op_buf       = req.buffer;
        op_sector    = req.sector;
        op_remaining = req.count;
        op_caller    = req.caller;
        op_p_result  = req.p_result;
        if (addr_is_dma_capable(op_buf, op_remaining * SD_SECTOR_SIZE)) {
            drv_state = DRV_DMA_READ_WAIT;
            start_dma_read();
        } else {
            drv_state = DRV_READ_WAIT;
            start_read_chunk();
        }
        break;

    case SD_EVT_WRITE_REQ:
        if (xQueueReceive(sd_arg_queue, &req, 0) != pdTRUE) {
            sd_dbg_print("  sd_drv: WRITE_REQ but no args!\r\n");
            break;
        }
        op_buf       = req.buffer;
        op_sector    = req.sector;
        op_remaining = req.count;
        op_caller    = req.caller;
        op_p_result  = req.p_result;
        if (addr_is_dma_capable(op_buf, op_remaining * SD_SECTOR_SIZE)) {
            drv_state = DRV_DMA_WRITE_WAIT;
            start_dma_write();
        } else {
            drv_state = DRV_WRITE_WAIT;
            start_write_chunk();
        }
        break;

    case SD_EVT_CARD_REMOVE:
        // sd_dbg_print("  sd_drv: card removed\r\n");
        sd_card.initialised = 0;
        break;

    case SD_EVT_CARD_INSERT:
        // sd_dbg_print("  sd_drv: card inserted, re-init\r\n");
        sd_hw_setup();
        if (sd_card_init()) {
            if (sd_try_high_speed()) {
                REG32(SD_CLOCK_CTRL) = 0x000E0105;
            } else {
                REG32(SD_CLOCK_CTRL) = 0x000E0205;
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        sd_hw_clear_interrupts(sd_event_queue);
        break;

    default:
        // Stale interrupt events while idle — silently discard
        break;

    }
}

static void handle_read_wait(uint8_t evt)
{
    switch (evt) {

    case SD_EVT_XFER_COMPLETE:
        sd_read_complete((uint32_t *)op_buf, op_chunk);
        advance_chunk();
        if (op_remaining > 0) {
            start_read_chunk();
        } else {
            finish_op(0);
        }
        break;

    case SD_EVT_ERR_CMD_TIMEOUT:
    case SD_EVT_ERR_CMD_CRC:
    case SD_EVT_ERR_CMD_END_BIT:
    case SD_EVT_ERR_CMD_INDEX:
    case SD_EVT_ERR_DATA_TIMEOUT:
    case SD_EVT_ERR_DATA_CRC:
    case SD_EVT_ERR_DATA_END_BIT:
    case SD_EVT_ERR_AUTO_CMD12:
        sd_dbg_printf("  sd_drv: read error evt=%u\r\n", (unsigned)evt);
        abort_op();
        break;

    case SD_EVT_CARD_REMOVE:
        // sd_dbg_print("  sd_drv: card removed during read\r\n");
        sd_card.initialised = 0;
        abort_op();
        break;

    default:
        // CMD_COMPLETE, BUF_READ_READY, etc. — expected, ignore
        break;

    }
}

static void handle_write_wait(uint8_t evt)
{
    switch (evt) {

    case SD_EVT_XFER_COMPLETE:
        sd_write_complete(op_chunk);
        advance_chunk();
        if (op_remaining > 0) {
            start_write_chunk();
        } else {
            finish_op(0);
        }
        break;

    case SD_EVT_ERR_CMD_TIMEOUT:
    case SD_EVT_ERR_CMD_CRC:
    case SD_EVT_ERR_CMD_END_BIT:
    case SD_EVT_ERR_CMD_INDEX:
    case SD_EVT_ERR_DATA_TIMEOUT:
    case SD_EVT_ERR_DATA_CRC:
    case SD_EVT_ERR_DATA_END_BIT:
    case SD_EVT_ERR_AUTO_CMD12:
        sd_dbg_printf("  sd_drv: write error evt=%u\r\n", (unsigned)evt);
        abort_op();
        break;

    case SD_EVT_CARD_REMOVE:
        // sd_dbg_print("  sd_drv: card removed during write\r\n");
        sd_card.initialised = 0;
        abort_op();
        break;

    default:
        // CMD_COMPLETE, BUF_WRITE_READY, etc. — expected, ignore
        break;

    }
}

static void handle_dma_read_wait(uint8_t evt)
{
    switch (evt) {

    case SD_EVT_DMA_INT:
        // SDMA boundary crossing — advance SDMA address to resume.
        // The SDMA engine pauses after each boundary; writing the next
        // address restarts it.
        REG32(SD_SDMA_ADDR) = REG32(SD_SDMA_ADDR);
        break;

    case SD_EVT_XFER_COMPLETE:
        sd_read_complete_dma(op_chunk);
        // Invalidate dcache so CPU reads DMA'd data from DDR
        riscv_invalidate_dcache_range((unsigned int)op_buf,
                                      op_chunk * SD_SECTOR_SIZE);
        advance_chunk();
        if (op_remaining > 0) {
            start_dma_read();
        } else {
            finish_op(0);
        }
        break;

    case SD_EVT_ERR_CMD_TIMEOUT:
    case SD_EVT_ERR_CMD_CRC:
    case SD_EVT_ERR_CMD_END_BIT:
    case SD_EVT_ERR_CMD_INDEX:
    case SD_EVT_ERR_DATA_TIMEOUT:
    case SD_EVT_ERR_DATA_CRC:
    case SD_EVT_ERR_DATA_END_BIT:
    case SD_EVT_ERR_AUTO_CMD12:
        sd_dbg_printf("  sd_drv: DMA read error evt=%u\r\n", (unsigned)evt);
        abort_op();
        break;

    case SD_EVT_CARD_REMOVE:
        sd_card.initialised = 0;
        abort_op();
        break;

    default:
        // CMD_COMPLETE, etc. — expected, ignore
        break;

    }
}

static void handle_dma_write_wait(uint8_t evt)
{
    switch (evt) {

    case SD_EVT_DMA_INT:
        // SDMA boundary crossing — advance SDMA address to resume.
        REG32(SD_SDMA_ADDR) = REG32(SD_SDMA_ADDR);
        break;

    case SD_EVT_XFER_COMPLETE:
        sd_write_complete_dma(op_chunk);
        advance_chunk();
        if (op_remaining > 0) {
            start_dma_write();
        } else {
            finish_op(0);
        }
        break;

    case SD_EVT_ERR_CMD_TIMEOUT:
    case SD_EVT_ERR_CMD_CRC:
    case SD_EVT_ERR_CMD_END_BIT:
    case SD_EVT_ERR_CMD_INDEX:
    case SD_EVT_ERR_DATA_TIMEOUT:
    case SD_EVT_ERR_DATA_CRC:
    case SD_EVT_ERR_DATA_END_BIT:
    case SD_EVT_ERR_AUTO_CMD12:
        sd_dbg_printf("  sd_drv: DMA write error evt=%u\r\n", (unsigned)evt);
        abort_op();
        break;

    case SD_EVT_CARD_REMOVE:
        sd_card.initialised = 0;
        abort_op();
        break;

    default:
        // CMD_COMPLETE, etc. — expected, ignore
        break;

    }
}

// -----------------------------------------------------------------------
// Driver task
// -----------------------------------------------------------------------
static void sd_driver_task(void *pvParameters)
{
    uint8_t evt;
    TickType_t timeout;
    BaseType_t got;

    (void)pvParameters;

    // Allow system to settle
    vTaskDelay(pdMS_TO_TICKS(100));

    // sd_dbg_print("\r\n");
    // sd_dbg_print("========================================\r\n");
    // sd_dbg_print("  SD Driver (queue-based) starting\r\n");
    // sd_dbg_print("========================================\r\n\r\n");

    // Setup hardware and initialise card
    sd_hw_setup();

    if (!sd_card_init()) {
        sd_dbg_print("  sd_drv: card init FAILED\r\n");
        // Stay at 400 kHz — will re-init on card insert event
    } else {
        // Try High Speed mode (50 MHz); fall back to Default Speed (25 MHz)
        if (sd_try_high_speed()) {
            // freq_select=1: sd_clk = 100 MHz / (2 × 1) = 50 MHz, DTCV=0xE
            REG32(SD_CLOCK_CTRL) = 0x000E0105;
        } else {
            // freq_select=2: sd_clk = 100 MHz / (2 × 2) = 25 MHz, DTCV=0xE
            REG32(SD_CLOCK_CTRL) = 0x000E0205;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
        sd_hw_clear_interrupts(sd_event_queue);
    }

    // Enable SD IRQ in INTC
    INTC_ClearPendingIRQ(SD_CONTROLLER_IRQn);
    INTC_EnableIRQ(SD_CONTROLLER_IRQn);

    sd_hw_clear_interrupts(sd_event_queue);
    drv_state = DRV_IDLE;

    // sd_dbg_print("  sd_drv: entering main loop\r\n");

    while (1) {
        timeout = (drv_state == DRV_IDLE) ? portMAX_DELAY
                                          : pdMS_TO_TICKS(5000);
        got = xQueueReceive(sd_event_queue, &evt, timeout);

        if (got != pdTRUE) {
            // Timeout — only happens during an active IO operation
            sd_dbg_print("  sd_drv: IO timeout\r\n");
            abort_op();
        } else {
            switch (drv_state) {
            case DRV_IDLE:
                handle_idle(evt);
                break;
            case DRV_READ_WAIT:
                handle_read_wait(evt);
                break;
            case DRV_WRITE_WAIT:
                handle_write_wait(evt);
                break;
            case DRV_DMA_READ_WAIT:
                handle_dma_read_wait(evt);
                break;
            case DRV_DMA_WRITE_WAIT:
                handle_dma_write_wait(evt);
                break;
            }
        }
    }
}

// -----------------------------------------------------------------------
// Public API: sd_driver_init
// -----------------------------------------------------------------------
int sd_driver_init(void)
{
    // Initialise card state
    memset(&sd_card, 0, sizeof(sd_card));

    // Create queues (static allocation)
    sd_event_queue = xQueueCreateStatic(
        SD_EVENT_QUEUE_LEN, sizeof(uint8_t),
        event_q_store, &event_q_buf);

    sd_arg_queue = xQueueCreateStatic(
        SD_ARG_QUEUE_LEN, sizeof(sd_io_request_t),
        (uint8_t *)arg_q_store, &arg_q_buf);

    // Create driver task (static allocation)
    xTaskCreateStatic(sd_driver_task, "sd_drv",
                      SD_DRIVER_STACK_SIZE, NULL,
                      SD_DRIVER_TASK_PRIORITY,
                      driver_stack, &driver_tcb);

    return 1;
}

// -----------------------------------------------------------------------
// Public API: card info accessor
// -----------------------------------------------------------------------
const sd_card_info_t *sd_driver_get_card_info(void)
{
    // The sd_card_state_t and sd_card_info_t layouts are identical
    return (const sd_card_info_t *)&sd_card;
}
