// sd_driver_hw.c -- Low-level SD controller helpers
//
// These functions are called by the driver task during card init
// and during IO operations.  All interrupt events are received
// from sd_event_queue.

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <UART_16550.h>
#include "sd_driver_private.h"

// -----------------------------------------------------------------------
// Debug printing
// -----------------------------------------------------------------------
static char dbg_buf[128];

void sd_dbg_print(const char *s)
{
    UART_16550_write_string(UART0, (char *)s, portMAX_DELAY);
}

void sd_dbg_printf(const char *fmt_str, ...)
{
    va_list ap;

    va_start(ap, fmt_str);
    vsnprintf(dbg_buf, sizeof(dbg_buf), fmt_str, ap);
    va_end(ap);
    sd_dbg_print(dbg_buf);
}

// -----------------------------------------------------------------------
// Command register builder
// -----------------------------------------------------------------------
uint16_t sd_hw_make_cmd(uint8_t index, uint8_t resp,
                        int crc_check, int index_check)
{
    return ((uint16_t)index << 8)
         | (index_check ? (1 << 4) : 0)
         | (crc_check   ? (1 << 3) : 0)
         | (resp & 0x03);
}

// -----------------------------------------------------------------------
// Block argument (SDHC = sector number, SDSC = byte address)
// -----------------------------------------------------------------------
uint32_t sd_hw_block_arg(uint32_t sector)
{
    return sd_card.is_sdhc ? sector : (sector * SD_SECTOR_SIZE);
}

// -----------------------------------------------------------------------
// Discard stale interrupt events so sd_hw_wait_int starts clean
//
// Only the ISR touches the hardware status registers (W1C).  We
// let the ISR fire to clear any pending hardware status bits, then
// reset the queue to discard the stale events it posted.
// -----------------------------------------------------------------------
void sd_hw_clear_interrupts(QueueHandle_t drain_queue)
{
    // Let the ISR run to W1C-clear any pending hardware status bits.
    // Clear pending and re-enable so the INTC dispatches any latched IRQ.
    INTC_ClearPendingIRQ(SD_CONTROLLER_IRQn);
    INTC_EnableIRQ(SD_CONTROLLER_IRQn);

    // The ISR (if it ran) posted stale events.  Disable, discard,
    // then re-enable with a clean slate.
    INTC_DisableIRQ(SD_CONTROLLER_IRQn);

    if (drain_queue != NULL) {
        xQueueReset(drain_queue);
    }

    INTC_ClearPendingIRQ(SD_CONTROLLER_IRQn);
    INTC_EnableIRQ(SD_CONTROLLER_IRQn);
}

// Last error status captured by sd_hw_wait_int (for diagnostics)
static uint16_t last_err_status;

uint16_t sd_hw_get_last_err(void)
{
    return last_err_status;
}

// -----------------------------------------------------------------------
// Wait for interrupt events matching 'mask'
//
// Blocks on 'int_queue', receiving individual uint8_t interrupt events
// and accumulating a bitmask until the requested mask is satisfied or
// the timeout expires.
//
// Returns the combined normal status (with SD_INT_ERROR set if any
// error events arrived), or 0 on timeout.
// -----------------------------------------------------------------------
uint16_t sd_hw_wait_int(uint16_t mask, uint32_t timeout_ms,
                        QueueHandle_t int_queue)
{
    TickType_t deadline;
    uint16_t acc_norm = 0;
    uint16_t acc_err  = 0;
    TickType_t now;
    TickType_t remaining;
    uint8_t evt;
    uint16_t combined;

    // Make sure the INTC is enabled (the ISR bail-out may have
    // disabled it on a previous spurious-interrupt storm).
    INTC_ClearPendingIRQ(SD_CONTROLLER_IRQn);
    INTC_EnableIRQ(SD_CONTROLLER_IRQn);

    deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);

    while (1) {
        now = xTaskGetTickCount();
        remaining = (now < deadline) ? (deadline - now) : 0;
        if (remaining == 0) {
            last_err_status = acc_err;
            return 0;
        }

        if (xQueueReceive(int_queue, &evt, remaining) != pdTRUE) {
            last_err_status = acc_err;
            return 0;  // timeout
        }

        // Map the uint8_t event back to bitmask positions.
        // Events 0-7 are normal status bits; 8-16 are error bits
        // (8+bit position; bit 7 reserved, bit 8 = Auto CMD12).
        // Events >= 20 (READ_REQ, WRITE_REQ) are ignored here.
        if (evt < 8) {
            acc_norm |= (uint16_t)(1 << evt);
        } else if (evt < 17) {
            acc_err |= (uint16_t)(1 << (evt - 8));
        }

        // Synthesise the error-summary bit
        combined = acc_norm;
        if (acc_err) {
            combined |= SD_INT_ERROR;
        }

        if (combined & mask) {
            last_err_status = acc_err;
            return combined;
        }
    }
}

// -----------------------------------------------------------------------
// Send a command and wait for cmd_complete
// -----------------------------------------------------------------------
int sd_hw_send_cmd(uint16_t cmd_reg, uint32_t argument,
                   uint16_t tm_hw, uint32_t timeout_ms,
                   QueueHandle_t int_queue)
{
    uint16_t ese;
    uint16_t esig;
    int wait;
    uint32_t tm_cmd;
    uint16_t norm;

    sd_hw_clear_interrupts(int_queue);

    // Verify and restore error enables if corrupted (known HW quirk)
    ese  = REG16(SD_ERR_INT_STAT_EN);
    esig = REG16(SD_ERR_INT_SIG_EN);
    if (ese == 0 || esig == 0) {
        REG16(SD_ERR_INT_STAT_EN) = 0x01FF;
        REG16(SD_ERR_INT_SIG_EN)  = 0x01FF;
    }

    // Wait for Command Inhibit (CMD) to clear
    wait = 0;
    while (REG32(SD_PRESENT_STATE) & SD_STATE_CMD_INHIBIT) {
        vTaskDelay(pdMS_TO_TICKS(1));
        if (++wait > 50) {
            sd_dbg_print("  sd_hw: cmd_inhibit stuck\r\n");
            return 0;
        }
    }

    // Write argument and command+transfer-mode as a single 32-bit
    // write (writing the command halfword triggers the engine).
    REG32(SD_ARGUMENT) = argument;
    tm_cmd = ((uint32_t)cmd_reg << 16) | tm_hw;
    REG32(SD_TRANSFER_MODE) = tm_cmd;

    // Wait for cmd_complete or error
    norm = sd_hw_wait_int(
        SD_INT_CMD_COMPLETE | SD_INT_ERROR, timeout_ms, int_queue);

    if (norm & SD_INT_CMD_COMPLETE) {
        return 1;  // success
    }

    // Diagnose failure
    if (norm & SD_INT_ERROR) {
        sd_dbg_printf("  sd_hw: cmd err norm=0x%04X err=0x%04X\r\n",
                      norm, sd_hw_get_last_err());
    } else {
        sd_dbg_printf("  sd_hw: cmd timeout (cmd_reg=0x%04X)\r\n", cmd_reg);
    }
    return 0;
}

// -----------------------------------------------------------------------
// Wait for cmd_inhibit_dat to clear after an R1b command (CMD12)
//
// The cmd engine's S_WAIT_BUSY state holds cmd_inhibit_dat high
// until DAT[0] releases.  If we proceed before it clears, the next
// Transfer Mode write is silently dropped (Sec.2.2.5 gating).
// -----------------------------------------------------------------------
void sd_hw_wait_dat_release(void)
{
    int wait = 0;

    while (REG32(SD_PRESENT_STATE) & SD_STATE_CMD_INHIBIT_DAT) {
        vTaskDelay(pdMS_TO_TICKS(1));
        if (++wait > 1000) {  // ~1 second
            sd_dbg_print("  sd_hw: cmd_inhibit_dat stuck, resetting\r\n");
            REG8(SD_SW_RESET) = SD_RESET_CMD | SD_RESET_DAT;
            (void)REG32(SD_CLOCK_CTRL);
            break;
        }
    }
}

// -----------------------------------------------------------------------
// Send CMD12 STOP_TRANSMISSION and wait for DAT release
//
// Used after multi-block transfers.  Resets the data engine first
// so cmd_inhibit_dat from the data phase does not block the CMD12.
// -----------------------------------------------------------------------
static void send_stop_cmd(QueueHandle_t int_queue)
{
    REG8(SD_SW_RESET) = SD_RESET_DAT;
    (void)REG32(SD_CLOCK_CTRL);

    sd_hw_send_cmd(sd_hw_make_cmd(12, 3, 1, 1),
                   0x00000000, 0x0000, 500, int_queue);

    sd_hw_wait_dat_release();
}

// -----------------------------------------------------------------------
// Burst write: 8 words per group via individual store to the same
// device address.  A multi-word store would generate an INCR burst
// that walks through register space.
// -----------------------------------------------------------------------
void sd_hw_burst_write(volatile uint32_t *dst,
                       const uint32_t *src, int groups)
{
    int i, j;

    for (i = 0; i < groups; i++) {
        for (j = 0; j < 8; j++) {
            *dst = *src++;
        }
    }
}

// -----------------------------------------------------------------------
// Burst read: 8 words per group via individual load from the same
// device address.
// -----------------------------------------------------------------------
void sd_hw_burst_read(const volatile uint32_t *src,
                      uint32_t *dst, int groups)
{
    int i, j;

    for (i = 0; i < groups; i++) {
        for (j = 0; j < 8; j++) {
            *dst++ = *src;
        }
    }
}

// -----------------------------------------------------------------------
// Public: issue CMD12 after a multi-block transfer.
// -----------------------------------------------------------------------
void sd_hw_stop_transmission(QueueHandle_t int_queue)
{
    send_stop_cmd(int_queue);
}
