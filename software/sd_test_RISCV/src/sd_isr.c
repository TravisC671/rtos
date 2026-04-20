// sd_isr.c -- SD Controller interrupt service routine
//
// The ISR reads and W1C-clears the SD controller interrupt status
// registers, accumulates the bits in volatile globals, and gives
// binary semaphores to wake any waiting task.
//
// On RISC-V (MicroBlaze V), the ISR is registered with the AXI INTC
// via INTC_SetVector rather than using a weak symbol override.

#include <stdint.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <interrupts.h>
#include <device_addrs.h>
#include <sd_isr.h>

// -----------------------------------------------------------------------
// Semaphore handles
// -----------------------------------------------------------------------
SemaphoreHandle_t sd_sem_cmd_complete;
SemaphoreHandle_t sd_sem_xfer_complete;
SemaphoreHandle_t sd_sem_dma_int;
SemaphoreHandle_t sd_sem_card_insert;
SemaphoreHandle_t sd_sem_card_remove;
SemaphoreHandle_t sd_sem_error;

// Static storage for the semaphores (no dynamic allocation)
static StaticSemaphore_t sd_sem_cmd_buf;
static StaticSemaphore_t sd_sem_xfer_buf;
static StaticSemaphore_t sd_sem_dma_buf;
static StaticSemaphore_t sd_sem_card_ins_buf;
static StaticSemaphore_t sd_sem_card_rem_buf;
static StaticSemaphore_t sd_sem_err_buf;

// -----------------------------------------------------------------------
// Accumulated interrupt status (set by ISR, read/cleared by task)
// -----------------------------------------------------------------------
volatile uint16_t sd_isr_norm_status;
volatile uint16_t sd_isr_err_status;

// -----------------------------------------------------------------------
// ISR — registered with AXI INTC via INTC_SetVector(SD_IRQ, ...)
// -----------------------------------------------------------------------
void sd_isr_handler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Bail-out counter: if we re-enter with nothing actionable, the
    // interrupt source is stuck.  After a few passes, disable the IRQ
    // in the AXI INTC.
    static volatile int empty_count = 0;

    // Read raw interrupt status registers
    uint16_t norm = REG16(SD_NORM_INT_STATUS);
    uint16_t err  = REG16(SD_ERR_INT_STATUS);

    // W1C clear ALL bits using two separate 16-bit writes.
    REG16(SD_NORM_INT_STATUS) = 0x00FF;   // W1C normal status
    REG16(SD_ERR_INT_STATUS)  = 0x01FF;   // W1C error status (incl. Auto CMD12 bit 8)

    // Read-back fence: ensure the AXI write has propagated through
    // the register bank and the interrupt output has deasserted
    // before we return from the ISR.
    (void)REG16(SD_NORM_INT_STATUS);

    // Only consider W1C-able bits for semaphore decisions (mask out bit 15)
    uint16_t norm_en = REG16(SD_NORM_INT_STAT_EN);
    uint16_t err_en  = REG16(SD_ERR_INT_STAT_EN);
    uint16_t norm_masked = norm & norm_en & 0x00FF;
    uint16_t err_masked  = err  & err_en;

    // Check if we have anything actionable
    if (norm_masked == 0 && err_masked == 0 && (norm & 0x00FF) == 0 && err == 0) {
        // Nothing to do — interrupt source is unknown/unclearable
        if (++empty_count >= 3) {
            // Disable this IRQ in the INTC to prevent infinite loop.
            // Task code will re-enable when it needs SD interrupts.
            INTC_DisableIRQ(SD_IRQ);
            INTC_ClearPendingIRQ(SD_IRQ);
            empty_count = 0;
            return;
        }
        // Give the W1C one more cycle to take effect
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        return;
    }

    empty_count = 0;  // reset on any real activity

    // Accumulate into volatile globals (task will read and clear)
    sd_isr_norm_status |= norm_masked;
    sd_isr_err_status  |= err_masked;

    // Give appropriate semaphores

    if (norm_masked & SD_INT_CMD_COMPLETE)
        xSemaphoreGiveFromISR(sd_sem_cmd_complete, &xHigherPriorityTaskWoken);

    if (norm_masked & SD_INT_XFER_COMPLETE)
        xSemaphoreGiveFromISR(sd_sem_xfer_complete, &xHigherPriorityTaskWoken);

    if (norm_masked & SD_INT_DMA_INT)
        xSemaphoreGiveFromISR(sd_sem_dma_int, &xHigherPriorityTaskWoken);

    if (norm_masked & SD_INT_CARD_INSERT)
        xSemaphoreGiveFromISR(sd_sem_card_insert, &xHigherPriorityTaskWoken);

    if (norm_masked & SD_INT_CARD_REMOVE)
        xSemaphoreGiveFromISR(sd_sem_card_remove, &xHigherPriorityTaskWoken);

    if ((norm & (1u << 15)) || err_masked)
        xSemaphoreGiveFromISR(sd_sem_error, &xHigherPriorityTaskWoken);

    // If a higher-priority task was woken, request a context switch
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// -----------------------------------------------------------------------
// Initialization
// -----------------------------------------------------------------------
void sd_isr_init(void)
{
    // Step 1: Create binary semaphores FIRST (static allocation)
    sd_sem_cmd_complete  = xSemaphoreCreateBinaryStatic(&sd_sem_cmd_buf);
    sd_sem_xfer_complete = xSemaphoreCreateBinaryStatic(&sd_sem_xfer_buf);
    sd_sem_dma_int       = xSemaphoreCreateBinaryStatic(&sd_sem_dma_buf);
    sd_sem_card_insert   = xSemaphoreCreateBinaryStatic(&sd_sem_card_ins_buf);
    sd_sem_card_remove   = xSemaphoreCreateBinaryStatic(&sd_sem_card_rem_buf);
    sd_sem_error         = xSemaphoreCreateBinaryStatic(&sd_sem_err_buf);

    // Step 2: Clear accumulated status
    sd_isr_norm_status = 0;
    sd_isr_err_status  = 0;

    // Step 3: Clear any pending interrupts in the SD controller hardware
    REG16(SD_NORM_INT_STATUS) = 0x00FF;
    REG16(SD_ERR_INT_STATUS)  = 0x01FF;  // incl. Auto CMD12 bit 8

    // Step 4: Register the ISR with the AXI INTC and enable it.
    // AXI INTC has fixed priorities (lower IRQ number = higher priority).
    INTC_SetVector(SD_IRQ, sd_isr_handler);
    INTC_ClearPendingIRQ(SD_IRQ);
    INTC_EnableIRQ(SD_IRQ);
}

// -----------------------------------------------------------------------
// Convenience: wait for an interrupt event matching 'mask'
//
// Waits on the appropriate semaphore(s).  Returns the accumulated
// normal status (which may contain more bits than requested).
// Returns 0 on timeout.
// -----------------------------------------------------------------------
uint16_t sd_wait_int(uint16_t mask, uint32_t timeout_ms)
{
    // Ensure the SD IRQ is enabled (ISR bail-out may have disabled it)
    INTC_ClearPendingIRQ(SD_IRQ);
    INTC_EnableIRQ(SD_IRQ);

    TickType_t ticks = pdMS_TO_TICKS(timeout_ms);
    TickType_t start = xTaskGetTickCount();
    TickType_t remaining = ticks;

    while (remaining > 0) {
        // Pick the best semaphore to wait on based on the mask
        SemaphoreHandle_t sem = NULL;

        if (mask & SD_INT_XFER_COMPLETE)
            sem = sd_sem_xfer_complete;
        else if (mask & SD_INT_CMD_COMPLETE)
            sem = sd_sem_cmd_complete;
        else if (mask & SD_INT_DMA_INT)
            sem = sd_sem_dma_int;
        else if (mask & SD_INT_CARD_INSERT)
            sem = sd_sem_card_insert;
        else if (mask & SD_INT_CARD_REMOVE)
            sem = sd_sem_card_remove;
        else if (mask & SD_INT_ERROR)
            sem = sd_sem_error;
        else
            sem = sd_sem_cmd_complete;  // fallback

        // Block until semaphore given or timeout
        xSemaphoreTake(sem, remaining);

        // Check if any requested bits have fired
        uint16_t norm = sd_isr_norm_status;
        uint16_t err  = sd_isr_err_status;

        // The error summary bit (15) in norm reflects err != 0
        if (err)
            norm |= SD_INT_ERROR;

        if (norm & mask) {
            return norm;
        }

        // Recalculate remaining time
        TickType_t elapsed = xTaskGetTickCount() - start;
        if (elapsed >= ticks)
            return 0;
        remaining = ticks - elapsed;
    }

    // Timeout
    return 0;
}
