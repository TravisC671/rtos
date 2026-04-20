// sd_driver_ff.c -- FreeRTOS+FAT integration and public block IO API
//
// Provides:
//   sd_driver_read_blocks()  - blocking sector read  (public API)
//   sd_driver_write_blocks() - blocking sector write (public API)
//   sd_ff_read_blocks()      - FF_ReadBlocks_t callback for FreeRTOS+FAT
//   sd_ff_write_blocks()     - FF_WriteBlocks_t callback for FreeRTOS+FAT
//
// These functions post a request to the unified event queue and
// block the calling task until the IO completes.  They are safe
// to call from any task context.

#include <stdint.h>
#include "sd_driver.h"
#include "sd_driver_private.h"

// -----------------------------------------------------------------------
// Internal: submit an IO request and wait for completion
//
// 1. Build sd_io_request_t with caller's task handle
// 2. Post it to the argument queue
// 3. Post a READ_REQ or WRITE_REQ event to the event queue
// 4. Block on ulTaskNotifyTake until the IO task notifies us
// 5. Return the result
// -----------------------------------------------------------------------
static int32_t submit_io(sd_evt_type_t type,
                         uint8_t *buffer,
                         uint32_t sector,
                         uint32_t count)
{
    volatile int32_t result;
    sd_io_request_t req;
    uint8_t evt;
    sd_io_request_t discard;

    if (!sd_card.initialised) {
        return -1;  // card not ready
    }
    if (count == 0) {
        return 0;   // nothing to do
    }

    result = -1;

    // Build the request
    req.buffer   = buffer;
    req.sector   = sector;
    req.count    = count;
    req.caller   = xTaskGetCurrentTaskHandle();
    req.p_result = &result;

    // Post arguments first, then the event.  The driver task
    // dequeues the event, then immediately dequeues the args.
    // The arg queue serialises concurrent callers: only one
    // request can be in flight at a time.
    if (xQueueSend(sd_arg_queue, &req, portMAX_DELAY) != pdTRUE) {
        return -1;
    }

    evt = (uint8_t)type;

    if (xQueueSend(sd_event_queue, &evt, portMAX_DELAY) != pdTRUE) {
        // Event post failed — try to recover the arg we just posted
        xQueueReceive(sd_arg_queue, &discard, 0);
        return -1;
    }

    // Block until the read or write task notifies us
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    return result;
}

// -----------------------------------------------------------------------
// Public API: read sectors
// -----------------------------------------------------------------------
int32_t sd_driver_read_blocks(uint8_t *buffer,
                              uint32_t sector,
                              uint32_t count)
{
    return submit_io(SD_EVT_READ_REQ, buffer, sector, count);
}

// -----------------------------------------------------------------------
// Public API: write sectors
// -----------------------------------------------------------------------
int32_t sd_driver_write_blocks(const uint8_t *buffer,
                               uint32_t sector,
                               uint32_t count)
{
    // The request struct uses uint8_t* (non-const) because the same
    // struct serves both reads and writes.  The write task treats
    // the buffer as const.
    return submit_io(SD_EVT_WRITE_REQ, (uint8_t *)buffer, sector, count);
}

// -----------------------------------------------------------------------
// FreeRTOS+FAT callbacks
//
// These match the FF_ReadBlocks_t / FF_WriteBlocks_t signatures:
//   int32_t fn(uint8_t *pucBuffer, uint32_t ulSectorAddress,
//              uint32_t ulCount, FF_Disk_t *pxDisk);
//
// Return 0 (FF_ERR_NONE) on success, or a negative error code.
// -----------------------------------------------------------------------

int32_t sd_ff_read_blocks(uint8_t *pucBuffer,
                          uint32_t ulSectorAddress,
                          uint32_t ulCount,
                          struct xFFDisk *pxDisk)
{
    (void)pxDisk;
    return sd_driver_read_blocks(pucBuffer, ulSectorAddress, ulCount);
}

int32_t sd_ff_write_blocks(uint8_t *pucBuffer,
                           uint32_t ulSectorAddress,
                           uint32_t ulCount,
                           struct xFFDisk *pxDisk)
{
    (void)pxDisk;
    return sd_driver_write_blocks(pucBuffer, ulSectorAddress, ulCount);
}
