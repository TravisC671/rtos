// sd_driver_isr.c -- SD Controller ISR for the queue-based driver
//
// Reads and W1C-clears the SD controller interrupt status registers,
// then posts one uint8_t event per set bit to the unified event queue.
// Error events are posted first (highest priority), then normal events.
//
// The vector table defines the handler as a weak symbol;
// SD_CONTROLLER_ISR_HANDLER (see sd_driver_private.h) maps to
// the correct name for this platform.

#include "sd_driver_private.h"

// -----------------------------------------------------------------------
// ISR — overrides the weak vector table entry for the SD controller
// -----------------------------------------------------------------------
void SD_CONTROLLER_ISR_HANDLER(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    static volatile int empty_count = 0;
    uint16_t norm;
    uint16_t err;
    uint16_t norm_en;
    uint16_t err_en;
    uint16_t norm_masked;
    uint16_t err_masked;
    uint8_t evt;
    int bit;

    // Read raw interrupt status registers
    norm = REG16(SD_NORM_INT_STATUS);
    err  = REG16(SD_ERR_INT_STATUS);

    // W1C clear ALL bits using two separate 16-bit writes.
    REG16(SD_NORM_INT_STATUS) = 0x00FF;
    REG16(SD_ERR_INT_STATUS)  = 0x01FF;   // incl. Auto CMD12 bit 8

    // Read-back fence: ensure the AXI write has propagated and the
    // interrupt output has deasserted before we return to the INTC.
    (void)REG16(SD_NORM_INT_STATUS);

    // Mask against the status-enable registers
    norm_en     = REG16(SD_NORM_INT_STAT_EN);
    err_en      = REG16(SD_ERR_INT_STAT_EN);
    norm_masked = norm & norm_en & 0x00FF;
    err_masked  = err  & err_en;

    // Check whether anything actionable fired
    if (norm_masked == 0 && err_masked == 0 &&
        (norm & 0x00FF) == 0 && err == 0)
    {
        if (++empty_count >= 3) {
            INTC_DisableIRQ(SD_CONTROLLER_IRQn);
            INTC_ClearPendingIRQ(SD_CONTROLLER_IRQn);
            empty_count = 0;
        }
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        return;
    }

    empty_count = 0;

    // Post error events first (highest priority).
    // Each set bit becomes a separate uint8_t event = 8 + bit position.
    // Bits 0-6 are standard errors, bit 7 is reserved, bit 8 is Auto CMD12.
    for (bit = 0; bit < 9; bit++) {
        if (err_masked & (1 << bit)) {
            evt = (uint8_t)(8 + bit);
            xQueueSendFromISR(sd_event_queue, &evt,
                              &xHigherPriorityTaskWoken);
        }
    }

    // Post normal events (value = bit position).
    for (bit = 0; bit < 8; bit++) {
        if (norm_masked & (1 << bit)) {
            evt = (uint8_t)bit;
            xQueueSendFromISR(sd_event_queue, &evt,
                              &xHigherPriorityTaskWoken);
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
