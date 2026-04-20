// sd_driver_private.h -- Internal types shared between driver components
//
// NOT part of the public API.  Only included by the driver source files.

#ifndef SD_DRIVER_PRIVATE_H
#define SD_DRIVER_PRIVATE_H

#include <stdint.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>
#include <interrupts.h>
#include <device_addrs.h>

// -----------------------------------------------------------------------
// IRQ mapping -- AXI INTC input assignments for MicroBlaze V platform
// -----------------------------------------------------------------------
#define SD_CONTROLLER_IRQn            SD_IRQ
#define SD_CONTROLLER_ISR_HANDLER     sd_controller_isr
void sd_controller_isr(void);

// -----------------------------------------------------------------------
// Event types for the unified event queue
//
// Each event is a single uint8_t.  Normal interrupt events use values
// matching the Normal Interrupt Status register bit positions (0-7).
// Error interrupt events use values 8 + Error Interrupt Status bit
// position (8-14).  Application events follow (15-16).
//
// The ISR posts one event per set interrupt bit, errors first, so
// the queue serializes everything without packing bits into structs.
// -----------------------------------------------------------------------
typedef enum {
    // Normal Interrupt Status bits (value = bit position)
    SD_EVT_CMD_COMPLETE     = 0,
    SD_EVT_XFER_COMPLETE    = 1,
    SD_EVT_DMA_INT          = 3,
    SD_EVT_BUF_WRITE_READY  = 4,
    SD_EVT_BUF_READ_READY   = 5,
    SD_EVT_CARD_INSERT       = 6,
    SD_EVT_CARD_REMOVE       = 7,
    // Error Interrupt Status bits (value = 8 + bit position)
    SD_EVT_ERR_CMD_TIMEOUT   = 8,
    SD_EVT_ERR_CMD_CRC       = 9,
    SD_EVT_ERR_CMD_END_BIT   = 10,
    SD_EVT_ERR_CMD_INDEX     = 11,
    SD_EVT_ERR_DATA_TIMEOUT  = 12,
    SD_EVT_ERR_DATA_CRC      = 13,
    SD_EVT_ERR_DATA_END_BIT  = 14,
    // Error bit 7 (current limit) is reserved/unused = event 15
    SD_EVT_ERR_AUTO_CMD12    = 16,  // err bit 8
    // Application events (must not collide with ISR events 0-16)
    SD_EVT_READ_REQ          = 20,
    SD_EVT_WRITE_REQ         = 21
} sd_evt_type_t;

// -----------------------------------------------------------------------
// IO request (FF_ReadBlocks / FF_WriteBlocks → driver task)
//
// The caller fills this, posts it to the argument queue, then posts
// a READ_REQ or WRITE_REQ event.  It blocks until the driver task
// sends a direct-to-task notification on 'caller'.
// -----------------------------------------------------------------------
typedef struct {
    uint8_t    *buffer;     // RAM source (write) or destination (read)
    uint32_t    sector;     // starting sector number
    uint32_t    count;      // number of 512-byte sectors
    TaskHandle_t caller;    // task to notify on completion
    volatile int32_t *p_result;  // where to write the return code
} sd_io_request_t;

// -----------------------------------------------------------------------
// Queue handles  (defined in sd_driver_task.c)
// -----------------------------------------------------------------------

// Unified event queue: ISR and FF layer post here.
// The driver task blocks on this queue and dispatches events.
extern QueueHandle_t sd_event_queue;

// Argument queue: FF layer posts sd_io_request_t here.
extern QueueHandle_t sd_arg_queue;

// -----------------------------------------------------------------------
// Queue sizing
// -----------------------------------------------------------------------
#define SD_EVENT_QUEUE_LEN        20
#define SD_ARG_QUEUE_LEN           1

// -----------------------------------------------------------------------
// Task priority and stack size
// -----------------------------------------------------------------------
#define SD_DRIVER_TASK_PRIORITY    4
#define SD_DRIVER_STACK_SIZE     1024

// -----------------------------------------------------------------------
// Transfer geometry
// -----------------------------------------------------------------------
#define SD_SECTOR_SIZE           512
#define SD_WORDS_PER_SECTOR      (SD_SECTOR_SIZE / 4)       // 128
#define SD_MAX_SECTORS_PER_XFER  4                           // 2KB buffer
#define SD_WORDS_PER_XFER        (SD_MAX_SECTORS_PER_XFER * SD_WORDS_PER_SECTOR)
#define SD_BURSTS_PER_XFER       (SD_WORDS_PER_XFER / 8)    // for LDM/STM

// -----------------------------------------------------------------------
// Shared card state  (written by driver task during init)
// -----------------------------------------------------------------------
typedef struct {
    uint16_t rca;
    int      is_sdhc;
    int      initialised;
    uint32_t total_sectors;
} sd_card_state_t;

extern sd_card_state_t sd_card;

// -----------------------------------------------------------------------
// Hardware helpers  (sd_driver_hw.c)
// -----------------------------------------------------------------------

// Build a command register value.
// resp: 0=none, 1=R2(136), 2=R1(48), 3=R1b(48+busy)
uint16_t sd_hw_make_cmd(uint8_t index, uint8_t resp,
                        int crc_check, int index_check);

// Compute the SD argument for a given sector number, accounting
// for SDHC (block addressing) vs SDSC (byte addressing).
uint32_t sd_hw_block_arg(uint32_t sector);

// Discard stale interrupt events from the specified queue and
// reset the INTC.  Does not touch hardware status registers
// (the ISR owns W1C clearing).
void sd_hw_clear_interrupts(QueueHandle_t drain_queue);

// Send a command and wait for cmd_complete on 'int_queue'.
//   cmd_reg : value for the command register (from sd_hw_make_cmd)
//   argument: 32-bit command argument
//   tm_hw   : Transfer Mode halfword (0 for command-only)
//   timeout : milliseconds to wait for cmd_complete
//   int_queue: queue to receive SD_EVT_INT messages from
// Returns 1 on success, 0 on error/timeout.
int sd_hw_send_cmd(uint16_t cmd_reg, uint32_t argument,
                   uint16_t tm_hw, uint32_t timeout_ms,
                   QueueHandle_t int_queue);

// Wait for interrupt events matching 'mask' on 'int_queue'.
// Returns the combined normal status (with SD_INT_ERROR set if
// any error bits are present), or 0 on timeout.
uint16_t sd_hw_wait_int(uint16_t mask, uint32_t timeout_ms,
                        QueueHandle_t int_queue);

// Wait for cmd_inhibit_dat to clear (R1b busy release).
// Issues a data-engine reset + CMD12 if it times out.
void sd_hw_wait_dat_release(void);

// Burst write 'groups' sets of 8 words from RAM to the buffer
// data port.  Uses individual STR instructions to avoid INCR
// bursts that would walk through register space.
void sd_hw_burst_write(volatile uint32_t *dst,
                       const uint32_t *src, int groups);

// Burst read 'groups' sets of 8 words from the buffer data port
// into RAM.
void sd_hw_burst_read(const volatile uint32_t *src,
                      uint32_t *dst, int groups);

// Send CMD12 STOP_TRANSMISSION after a multi-block transfer.
// Resets the data engine, sends CMD12, waits for DAT release,
// and clears stale interrupt status.
void sd_hw_stop_transmission(QueueHandle_t int_queue);

// Return the error status bits captured during the most recent
// sd_hw_wait_int call.  Useful for diagnostics when SD_INT_ERROR
// is set in the returned normal status.
uint16_t sd_hw_get_last_err(void);

// -----------------------------------------------------------------------
// Two-phase IO helpers called by the driver task event loop
// (sd_driver_read.c, sd_driver_write.c)
//
// The _start functions issue the SD command (CMD17/CMD18/CMD24/CMD25).
// The driver task returns to its event loop and waits for
// XFER_COMPLETE or an error event.
// The _complete functions handle the data transfer and CMD12 stop
// (for multi-block).
// -----------------------------------------------------------------------

// Issue CMD17 (count==1) or CMD18 (count>1) read command (PIO).
void sd_read_start(uint32_t sector, uint32_t count);

// Burst-read the buffer into dst and send CMD12 if multi-block (PIO).
void sd_read_complete(uint32_t *dst, uint32_t count);

// Fill the data buffer from src and issue CMD24 (count==1) or
// CMD25 (count>1) write command (PIO).
void sd_write_start(uint32_t sector, uint32_t count,
                    const uint32_t *src);

// Send CMD12 if multi-block (PIO).
void sd_write_complete(uint32_t count);

// -----------------------------------------------------------------------
// SDMA + Auto CMD12 variants
//
// These use the SDMA engine to transfer data between the data buffer
// and system memory, and Auto CMD12 for multi-block stop.  No PIO
// burst read/write or software CMD12 is needed.
// -----------------------------------------------------------------------

// Issue CMD17/CMD18 with DMA_ENABLE and Auto CMD12 (multi-block).
// buf_addr is the physical RAM address for SDMA.
void sd_read_start_dma(uint32_t sector, uint32_t count,
                       uint32_t buf_addr);

// DMA read complete — no PIO drain needed, no CMD12 needed.
void sd_read_complete_dma(uint32_t count);

// Issue CMD24/CMD25 with DMA_ENABLE and Auto CMD12 (multi-block).
// buf_addr is the physical RAM address for SDMA.
void sd_write_start_dma(uint32_t sector, uint32_t count,
                        uint32_t buf_addr);

// DMA write complete — no CMD12 needed.
void sd_write_complete_dma(uint32_t count);

// -----------------------------------------------------------------------
// Debug printing (uses UART_16550 on UART0)
// -----------------------------------------------------------------------
void sd_dbg_print(const char *s);
void sd_dbg_printf(const char *fmt_str, ...);

#endif // SD_DRIVER_PRIVATE_H
