// sd_isr.h -- SD Controller interrupt service routine and semaphores
#ifndef SD_ISR_H
#define SD_ISR_H

#include <FreeRTOS.h>
#include <semphr.h>

// Binary semaphores signaled by the SD controller ISR.
// Each corresponds to a class of SD interrupt events.
extern SemaphoreHandle_t sd_sem_cmd_complete;   // Normal Int bit 0
extern SemaphoreHandle_t sd_sem_xfer_complete;  // Normal Int bit 1
extern SemaphoreHandle_t sd_sem_dma_int;        // Normal Int bit 3
extern SemaphoreHandle_t sd_sem_card_insert;    // Normal Int bit 6
extern SemaphoreHandle_t sd_sem_card_remove;    // Normal Int bit 7
extern SemaphoreHandle_t sd_sem_error;          // Normal Int bit 15 (any error)

// Last captured interrupt status values (set by ISR before giving semaphores)
extern volatile uint16_t sd_isr_norm_status;
extern volatile uint16_t sd_isr_err_status;

// Initialize semaphores and enable SD controller IRQ in the NVIC.
// Call this once before starting the scheduler or before any SD tests.
void sd_isr_init(void);

// Wait for any of the specified normal interrupt bits to be set,
// using the appropriate semaphore(s).  Returns the captured normal
// status, or 0 on timeout.
//
// Common usage:
//   sd_wait_int(SD_INT_CMD_COMPLETE, timeout_ms)
//   sd_wait_int(SD_INT_XFER_COMPLETE | SD_INT_ERROR, timeout_ms)
uint16_t sd_wait_int(uint16_t mask, uint32_t timeout_ms);

#endif // SD_ISR_H
