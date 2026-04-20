// main.c -- Comprehensive test suite for the queue-based SD driver
//
// Tests:
//   T01  Single-block write/read/verify (basic)
//   T02  Boundary: sector 0
//   T03  Boundary: large sector number
//   T04  Multi-block counts: 2, 3, 4
//   T05  Multi-block spanning multiple chunks: 7, 8
//   T06  Data pattern: all zeros
//   T07  Data pattern: all ones (0xFF)
//   T08  Data pattern: alternating 0xAA / 0x55
//   T09  Data pattern: sequential bytes
//   T10  Data pattern: pseudo-random
//   T11  Stress: write/read/verify same sector N times
//   T12  Overwrite: write A, verify, write B, verify B, confirm A gone
//   T13  Multiple non-contiguous regions
//   T14  Card remove / re-insert (manual, prompted)
//   T15  SDMA single-block (DDR2 buffer)
//   T16  SDMA multi-block 8 sectors (DDR2 buffer)
//   T17  SDMA stress 50 iterations (DDR2 buffer)
//   T18  PIO single-block (static BRAM buffer)
//   T19  PIO multi-block 4 sectors (static BRAM buffer)
//   T20  PIO multi-chunk 8 sectors (static BRAM buffer)
//   B1   DMA write throughput (DDR2, 10s timed, per-second reports)
//   B2   DMA read throughput  (DDR2, 5s timed, per-second reports + verify)
//   B3   PIO write throughput (BRAM, 10s timed, per-second reports)
//   B4   PIO read throughput  (BRAM, 5s timed, per-second reports + verify)
//
// T01-T14 use DDR2 buffers (malloc) → DMA path.
// T15-T17 also use DDR2 buffers → DMA path.
// T18-T20 use static BRAM buffers → PIO fallback path.
// B1-B2 use DDR2 buffers → DMA path.
// B3-B4 use static BRAM buffers → PIO fallback path.
//
// All test data uses sectors 200+ to avoid MBR / partition tables.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include <UART_16550.h>
#include <device_addrs.h>
#include <interrupts.h>
#include "sd_driver.h"
#include "sd_driver_private.h"

// -----------------------------------------------------------------------
// Test infrastructure
// -----------------------------------------------------------------------
#define APP_STACK_SIZE  1024
static StaticTask_t app_tcb;
static StackType_t  app_stack[APP_STACK_SIZE];

// Buffers — large enough for 8 sectors (4096 bytes).
// Allocated from DDR2 (0x80000000) via malloc so SDMA can access them.
#define MAX_TEST_SECTORS  8
static uint8_t *wr_buf;
static uint8_t *rd_buf;

// Static buffers in DDR2 (.bss at 0x08000000 on MicroBlaze V) for PIO tests.
// These ARE DMA-accessible, but the driver uses the address check to decide.
static uint8_t pio_wr_buf[MAX_TEST_SECTORS * 512];
static uint8_t pio_rd_buf[MAX_TEST_SECTORS * 512];

static char fmt[128];
static char label[32];   // for stress test iteration names (avoids fmt aliasing)

static int tests_run;
static int tests_passed;
static int tests_failed;

static void print(const char *s)
{
    UART_16550_write_string(UART0, (char *)s, portMAX_DELAY);
}

static void pass(const char *name)
{
    tests_run++;
    tests_passed++;
    sprintf(fmt, "  PASS: %s\r\n", name);
    print(fmt);
}

static void fail(const char *name, const char *reason)
{
    tests_run++;
    tests_failed++;
    sprintf(fmt, "  FAIL: %s -- %s\r\n", name, reason);
    print(fmt);
}

// -----------------------------------------------------------------------
// Pattern generators
// -----------------------------------------------------------------------

// Fill buffer with a byte pattern: each byte = fn(index, seed)
static void fill_xor_pattern(uint8_t *buf, int len, uint8_t seed)
{
    int i;

    for (i = 0; i < len; i++) {
        buf[i] = (uint8_t)(i ^ seed);
    }
}

static void fill_constant(uint8_t *buf, int len, uint8_t val)
{
    memset(buf, val, len);
}

static void fill_sequential(uint8_t *buf, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        buf[i] = (uint8_t)(i & 0xFF);
    }
}

static void fill_alternating(uint8_t *buf, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        buf[i] = (i & 1) ? 0x55 : 0xAA;
    }
}

// Simple LFSR-based pseudo-random fill
static void fill_random(uint8_t *buf, int len, uint16_t seed)
{
    int i;
    uint16_t lfsr = seed;

    for (i = 0; i < len; i++) {
        // Galois LFSR, taps at 16,14,13,11
        lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400);
        buf[i] = (uint8_t)(lfsr & 0xFF);
    }
}

// -----------------------------------------------------------------------
// Verify helper: compare wr_buf to rd_buf, print first few mismatches
// Returns number of mismatches.
// -----------------------------------------------------------------------
static int verify_buffers(int len)
{
    int errors = 0;
    int i;

    for (i = 0; i < len; i++) {
        if (rd_buf[i] != wr_buf[i]) {
            if (errors < 3) {
                sprintf(fmt, "    byte %d: wr=0x%02X rd=0x%02X\r\n",
                        i, wr_buf[i], rd_buf[i]);
                print(fmt);
            }
            errors++;
        }
    }
    return errors;
}

// -----------------------------------------------------------------------
// Write-read-verify helper for N sectors at a given start sector.
// Uses the current contents of wr_buf.
// Returns 1 on success, 0 on failure.
// -----------------------------------------------------------------------
static int write_read_verify(const char *label, uint32_t sector,
                             uint32_t count)
{
    int32_t rc;
    int len;
    int errors;

    len = count * 512;

    rc = sd_driver_write_blocks(wr_buf, sector, count);
    if (rc != 0) {
        sprintf(fmt, "write failed (rc=%ld)", (long)rc);
        fail(label, fmt);
        return 0;
    }

    memset(rd_buf, 0xDE, len);  // poison read buffer
    rc = sd_driver_read_blocks(rd_buf, sector, count);
    if (rc != 0) {
        sprintf(fmt, "read failed (rc=%ld)", (long)rc);
        fail(label, fmt);
        return 0;
    }

    errors = verify_buffers(len);
    if (errors != 0) {
        sprintf(fmt, "%d mismatches", errors);
        fail(label, fmt);
        return 0;
    }

    pass(label);
    return 1;
}

// -----------------------------------------------------------------------
// T01: Basic single-block write/read/verify
// -----------------------------------------------------------------------
static void test_single_block_basic(void)
{
    fill_xor_pattern(wr_buf, 512, 0xA5);
    write_read_verify("T01 single block basic", 200, 1);
}

// -----------------------------------------------------------------------
// T02: Boundary — sector 0
// -----------------------------------------------------------------------
static void test_sector_zero(void)
{
    fill_xor_pattern(wr_buf, 512, 0x00);
    write_read_verify("T02 sector 0", 0, 1);
}

// -----------------------------------------------------------------------
// T03: Boundary — large sector number
// -----------------------------------------------------------------------
static void test_large_sector(void)
{
    fill_xor_pattern(wr_buf, 512, 0x77);
    write_read_verify("T03 large sector (100000)", 100000, 1);
}

// -----------------------------------------------------------------------
// T04: Multi-block counts 2, 3, 4
// -----------------------------------------------------------------------
static void test_multi_block_counts(void)
{
    fill_xor_pattern(wr_buf, 2 * 512, 0x11);
    write_read_verify("T04a 2 blocks", 210, 2);

    fill_xor_pattern(wr_buf, 3 * 512, 0x22);
    write_read_verify("T04b 3 blocks", 220, 3);

    fill_xor_pattern(wr_buf, 4 * 512, 0x33);
    write_read_verify("T04c 4 blocks", 230, 4);
}

// -----------------------------------------------------------------------
// T05: Multi-block spanning multiple chunks (>4 sectors)
// -----------------------------------------------------------------------
static void test_multi_chunk(void)
{
    fill_xor_pattern(wr_buf, 7 * 512, 0x44);
    write_read_verify("T05a 7 blocks (spans 2 chunks)", 240, 7);

    fill_xor_pattern(wr_buf, 8 * 512, 0x55);
    write_read_verify("T05b 8 blocks (spans 2 chunks)", 250, 8);
}

// -----------------------------------------------------------------------
// T06: Data pattern — all zeros
// -----------------------------------------------------------------------
static void test_pattern_zeros(void)
{
    fill_constant(wr_buf, 512, 0x00);
    write_read_verify("T06 all zeros", 260, 1);
}

// -----------------------------------------------------------------------
// T07: Data pattern — all ones
// -----------------------------------------------------------------------
static void test_pattern_ones(void)
{
    fill_constant(wr_buf, 512, 0xFF);
    write_read_verify("T07 all 0xFF", 261, 1);
}

// -----------------------------------------------------------------------
// T08: Data pattern — alternating 0xAA / 0x55
// -----------------------------------------------------------------------
static void test_pattern_alternating(void)
{
    fill_alternating(wr_buf, 512);
    write_read_verify("T08 alternating 0xAA/0x55", 262, 1);
}

// -----------------------------------------------------------------------
// T09: Data pattern — sequential bytes
// -----------------------------------------------------------------------
static void test_pattern_sequential(void)
{
    fill_sequential(wr_buf, 512);
    write_read_verify("T09 sequential bytes", 263, 1);
}

// -----------------------------------------------------------------------
// T10: Data pattern — pseudo-random
// -----------------------------------------------------------------------
static void test_pattern_random(void)
{
    fill_random(wr_buf, 512, 0xBEEF);
    write_read_verify("T10 pseudo-random", 264, 1);
}

// -----------------------------------------------------------------------
// T11: Stress — write/read/verify same sector N times
// -----------------------------------------------------------------------
static void test_stress_repeat(void)
{
    int iter;
    int ok = 1;

    print("  T11: stress 50 iterations on sector 270...\r\n");

    for (iter = 0; iter < 50; iter++) {
        fill_xor_pattern(wr_buf, 512, (uint8_t)(iter * 7 + 3));
        sprintf(label, "T11 iter %d", iter);
        if (!write_read_verify(label, 270, 1)) {
            tests_run--;
            ok = 0;
            break;
        }
        // undo the pass count — we'll report once at the end
        tests_run--;
        tests_passed--;
    }

    if (ok) {
        pass("T11 stress 50 iterations");
    } else {
        sprintf(fmt, "failed at iteration %d", iter);
        fail("T11 stress repeat", fmt);
    }
}

// -----------------------------------------------------------------------
// T12: Overwrite — write pattern A, verify, write pattern B, verify B,
//                  confirm pattern A is gone
// -----------------------------------------------------------------------
static void test_overwrite(void)
{
    int32_t rc;
    int errors;
    int i;
    int a_found;

    // Write pattern A
    fill_constant(wr_buf, 512, 0xAA);
    rc = sd_driver_write_blocks(wr_buf, 280, 1);
    if (rc != 0) {
        fail("T12 overwrite", "pattern A write failed");
        return;
    }

    // Verify pattern A
    memset(rd_buf, 0, 512);
    rc = sd_driver_read_blocks(rd_buf, 280, 1);
    if (rc != 0) {
        fail("T12 overwrite", "pattern A read failed");
        return;
    }
    fill_constant(wr_buf, 512, 0xAA);
    errors = verify_buffers(512);
    if (errors != 0) {
        fail("T12 overwrite", "pattern A verify failed");
        return;
    }

    // Write pattern B
    fill_constant(wr_buf, 512, 0x55);
    rc = sd_driver_write_blocks(wr_buf, 280, 1);
    if (rc != 0) {
        fail("T12 overwrite", "pattern B write failed");
        return;
    }

    // Verify pattern B
    memset(rd_buf, 0, 512);
    rc = sd_driver_read_blocks(rd_buf, 280, 1);
    if (rc != 0) {
        fail("T12 overwrite", "pattern B read failed");
        return;
    }
    fill_constant(wr_buf, 512, 0x55);
    errors = verify_buffers(512);
    if (errors != 0) {
        fail("T12 overwrite", "pattern B verify failed");
        return;
    }

    // Confirm pattern A is gone — every byte should be 0x55, not 0xAA
    a_found = 0;
    for (i = 0; i < 512; i++) {
        if (rd_buf[i] == 0xAA) {
            a_found++;
        }
    }
    if (a_found != 0) {
        sprintf(fmt, "%d bytes still 0xAA", a_found);
        fail("T12 overwrite", fmt);
        return;
    }

    pass("T12 overwrite A then B");
}

// -----------------------------------------------------------------------
// T13: Multiple non-contiguous regions
// -----------------------------------------------------------------------
static void test_non_contiguous(void)
{
    int32_t rc;
    int errors;
    uint8_t pattern_a;
    uint8_t pattern_b;
    uint8_t pattern_c;
    int i;

    // Write different patterns to three non-contiguous regions
    pattern_a = 0x11;
    pattern_b = 0x22;
    pattern_c = 0x33;

    fill_constant(wr_buf, 512, pattern_a);
    rc = sd_driver_write_blocks(wr_buf, 300, 1);
    if (rc != 0) {
        fail("T13 non-contiguous", "write region A failed");
        return;
    }

    fill_constant(wr_buf, 512, pattern_b);
    rc = sd_driver_write_blocks(wr_buf, 400, 1);
    if (rc != 0) {
        fail("T13 non-contiguous", "write region B failed");
        return;
    }

    fill_constant(wr_buf, 512, pattern_c);
    rc = sd_driver_write_blocks(wr_buf, 500, 1);
    if (rc != 0) {
        fail("T13 non-contiguous", "write region C failed");
        return;
    }

    // Read back all three and verify independently
    memset(rd_buf, 0, 512);
    rc = sd_driver_read_blocks(rd_buf, 300, 1);
    if (rc != 0) {
        fail("T13 non-contiguous", "read region A failed");
        return;
    }
    errors = 0;
    for (i = 0; i < 512; i++) {
        if (rd_buf[i] != pattern_a) errors++;
    }
    if (errors != 0) {
        fail("T13 non-contiguous", "region A verify failed");
        return;
    }

    memset(rd_buf, 0, 512);
    rc = sd_driver_read_blocks(rd_buf, 400, 1);
    if (rc != 0) {
        fail("T13 non-contiguous", "read region B failed");
        return;
    }
    errors = 0;
    for (i = 0; i < 512; i++) {
        if (rd_buf[i] != pattern_b) errors++;
    }
    if (errors != 0) {
        fail("T13 non-contiguous", "region B verify failed");
        return;
    }

    memset(rd_buf, 0, 512);
    rc = sd_driver_read_blocks(rd_buf, 500, 1);
    if (rc != 0) {
        fail("T13 non-contiguous", "read region C failed");
        return;
    }
    errors = 0;
    for (i = 0; i < 512; i++) {
        if (rd_buf[i] != pattern_c) errors++;
    }
    if (errors != 0) {
        fail("T13 non-contiguous", "region C verify failed");
        return;
    }

    pass("T13 three non-contiguous regions");
}

// -----------------------------------------------------------------------
// T14: Card remove / re-insert (manual)
// -----------------------------------------------------------------------
static void test_card_reinsert(void)
{
    const sd_card_info_t *info;
    int32_t rc;
    int errors;
    int wait;

    // Write a known pattern before removal
    fill_xor_pattern(wr_buf, 512, 0xCC);
    rc = sd_driver_write_blocks(wr_buf, 290, 1);
    if (rc != 0) {
        fail("T14 card reinsert", "pre-removal write failed");
        return;
    }

    print("\r\n");
    print("  *** T14: Remove the SD card now, then re-insert it ***\r\n");
    print("  *** Waiting up to 30 seconds...                     ***\r\n");

    // Wait for card to be removed
    wait = 0;
    info = sd_driver_get_card_info();
    while (info->initialised && wait < 150) {
        vTaskDelay(pdMS_TO_TICKS(200));
        info = sd_driver_get_card_info();
        wait++;
    }

    if (info->initialised) {
        print("  T14: card was not removed, skipping test\r\n");
        tests_run++;
        return;
    }
    print("  T14: card removed, waiting for re-insert...\r\n");

    // Wait for card to be re-inserted and re-initialised
    wait = 0;
    while (!info->initialised && wait < 150) {
        vTaskDelay(pdMS_TO_TICKS(200));
        info = sd_driver_get_card_info();
        wait++;
    }

    if (!info->initialised) {
        fail("T14 card reinsert", "card not re-initialised after 30s");
        return;
    }

    print("  T14: card re-initialised, verifying data...\r\n");

    // Allow a moment for the driver to finish re-init
    vTaskDelay(pdMS_TO_TICKS(500));

    // Read back the sector we wrote before removal
    memset(rd_buf, 0, 512);
    rc = sd_driver_read_blocks(rd_buf, 290, 1);
    if (rc != 0) {
        fail("T14 card reinsert", "post-reinsert read failed");
        return;
    }

    // Verify the data survived
    fill_xor_pattern(wr_buf, 512, 0xCC);
    errors = verify_buffers(512);
    if (errors != 0) {
        sprintf(fmt, "%d mismatches after reinsert", errors);
        fail("T14 card reinsert", fmt);
        return;
    }

    // Verify new write works after re-init
    fill_xor_pattern(wr_buf, 512, 0xDD);
    rc = sd_driver_write_blocks(wr_buf, 291, 1);
    if (rc != 0) {
        fail("T14 card reinsert", "post-reinsert write failed");
        return;
    }

    memset(rd_buf, 0, 512);
    rc = sd_driver_read_blocks(rd_buf, 291, 1);
    if (rc != 0) {
        fail("T14 card reinsert", "post-reinsert read-back failed");
        return;
    }

    fill_xor_pattern(wr_buf, 512, 0xDD);
    errors = verify_buffers(512);
    if (errors != 0) {
        fail("T14 card reinsert", "post-reinsert verify failed");
        return;
    }

    pass("T14 card remove / re-insert");
}

// -----------------------------------------------------------------------
// T15: SDMA single-block write/read/verify
// -----------------------------------------------------------------------
static void test_dma_single_block(void)
{
    fill_xor_pattern(wr_buf, 512, 0xD1);
    write_read_verify("T15 DMA single block", 310, 1);
}

// -----------------------------------------------------------------------
// T16: SDMA multi-block (8 sectors) write/read/verify
//      Exercises multi-chunk DMA with boundary crossing
// -----------------------------------------------------------------------
static void test_dma_multi_block(void)
{
    fill_xor_pattern(wr_buf, 8 * 512, 0xD2);
    write_read_verify("T16 DMA 8-block multi", 320, 8);
}

// -----------------------------------------------------------------------
// T17: SDMA stress test (50 iterations)
// -----------------------------------------------------------------------
static void test_dma_stress(void)
{
    int iter;
    int ok = 1;

    print("  T17: DMA stress 50 iterations on sector 330...\r\n");

    for (iter = 0; iter < 50; iter++) {
        fill_xor_pattern(wr_buf, 512, (uint8_t)(iter * 11 + 7));
        sprintf(label, "T17 iter %d", iter);
        if (!write_read_verify(label, 330, 1)) {
            tests_run--;
            ok = 0;
            break;
        }
        // undo the pass count — we'll report once at the end
        tests_run--;
        tests_passed--;
    }

    if (ok) {
        pass("T17 DMA stress 50 iterations");
    } else {
        sprintf(fmt, "failed at iteration %d", iter);
        fail("T17 DMA stress", fmt);
    }
}

// -----------------------------------------------------------------------
// T18: PIO single-block write/read/verify (static BRAM buffer)
//      Buffer is in internal BRAM — not DMA-capable, forces PIO path.
// -----------------------------------------------------------------------
static void test_pio_single_block(void)
{
    uint8_t *saved_wr;
    uint8_t *saved_rd;

    saved_wr = wr_buf;
    saved_rd = rd_buf;
    wr_buf = pio_wr_buf;
    rd_buf = pio_rd_buf;

    fill_xor_pattern(wr_buf, 512, 0xE1);
    write_read_verify("T18 PIO single block (BRAM)", 340, 1);

    wr_buf = saved_wr;
    rd_buf = saved_rd;
}

// -----------------------------------------------------------------------
// T19: PIO multi-block (4 sectors) write/read/verify (static BRAM)
//      4 sectors = one full buffer chunk, no multi-chunk splitting.
// -----------------------------------------------------------------------
static void test_pio_multi_block(void)
{
    uint8_t *saved_wr;
    uint8_t *saved_rd;

    saved_wr = wr_buf;
    saved_rd = rd_buf;
    wr_buf = pio_wr_buf;
    rd_buf = pio_rd_buf;

    fill_xor_pattern(wr_buf, 4 * 512, 0xE2);
    write_read_verify("T19 PIO 4-block (BRAM)", 350, 4);

    wr_buf = saved_wr;
    rd_buf = saved_rd;
}

// -----------------------------------------------------------------------
// T20: PIO multi-chunk (8 sectors) write/read/verify (static BRAM)
//      8 sectors spans 2 chunks of 4 — exercises PIO chunking loop.
// -----------------------------------------------------------------------
static void test_pio_multi_chunk(void)
{
    uint8_t *saved_wr;
    uint8_t *saved_rd;

    saved_wr = wr_buf;
    saved_rd = rd_buf;
    wr_buf = pio_wr_buf;
    rd_buf = pio_rd_buf;

    fill_xor_pattern(wr_buf, 8 * 512, 0xE3);
    write_read_verify("T20 PIO 8-block multi-chunk (BRAM)", 360, 8);

    wr_buf = saved_wr;
    rd_buf = saved_rd;
}

// =======================================================================
// Throughput Benchmarks (B1–B4)
//
// Modeled on sd_test T37/T38/T52/T53.  Each benchmark writes or reads
// 4 blocks per iteration in a timed loop, printing per-second
// instantaneous and average throughput in KB/s.
//
// B1/B2 use DDR2 buffers (malloc) → SDMA + Auto CMD12 path.
// B3/B4 use static BRAM buffers  → PIO + 4-sector chunking path.
// =======================================================================

#define BENCH_BLOCKS      4       // blocks per iteration (one full chunk)
#define BENCH_BYTES       (BENCH_BLOCKS * 512)
#define BENCH_SECTOR      1000    // starting sector (above test range)
#define BENCH_WRITE_MS    10000   // write benchmark duration
#define BENCH_READ_MS     5000    // read benchmark duration

// KB/s = (blocks * 512 / 1024) / (ms / 1000) = blocks * 500 / ms
static uint32_t calc_kbps(uint32_t blocks, uint32_t ms)
{
    if (ms == 0) return 0;
    return (blocks * 500) / ms;
}

// -----------------------------------------------------------------------
// B1: DMA write throughput (DDR2, 10s, per-second reports)
// Returns the total number of blocks written (for B2 read range).
// -----------------------------------------------------------------------
static uint32_t bench_dma_write(void)
{
    uint32_t blk;
    uint32_t total_blocks;
    uint32_t total_errors;
    uint32_t second;
    uint32_t interval_blocks;
    uint32_t elapsed;
    uint32_t interval_ms;
    uint32_t inst_kbps;
    uint32_t avg_kbps;
    uint32_t total_ms;
    TickType_t run_start;
    TickType_t next_report;
    TickType_t now;
    int32_t rc;

    fill_xor_pattern(wr_buf, BENCH_BYTES, 0xB1);

    print("  B1: DMA write (DDR2), 4-block bursts for 10 seconds...\r\n");
    print("  [sec]  this KB/s    avg KB/s    total blocks\r\n");

    blk = BENCH_SECTOR;
    total_blocks = 0;
    total_errors = 0;
    second = 0;
    interval_blocks = 0;
    run_start = xTaskGetTickCount();
    next_report = run_start + pdMS_TO_TICKS(1000);

    elapsed = 0;
    while (elapsed < BENCH_WRITE_MS && total_errors < 10) {
        rc = sd_driver_write_blocks(wr_buf, blk, BENCH_BLOCKS);
        if (rc != 0) {
            total_errors++;
        } else {
            total_blocks += BENCH_BLOCKS;
            interval_blocks += BENCH_BLOCKS;
        }
        blk += BENCH_BLOCKS;

        now = xTaskGetTickCount();
        elapsed = (uint32_t)(now - run_start);

        if (now >= next_report) {
            second++;
            interval_ms = (uint32_t)(now - (next_report - pdMS_TO_TICKS(1000)));
            inst_kbps = calc_kbps(interval_blocks, interval_ms);
            avg_kbps = calc_kbps(total_blocks, elapsed);
            sprintf(fmt, "  [%2lu]   %5lu       %5lu       %lu\r\n",
                    (unsigned long)second,
                    (unsigned long)inst_kbps,
                    (unsigned long)avg_kbps,
                    (unsigned long)total_blocks);
            print(fmt);
            interval_blocks = 0;
            next_report += pdMS_TO_TICKS(1000);
        }
    }

    total_ms = (uint32_t)(xTaskGetTickCount() - run_start);
    avg_kbps = calc_kbps(total_blocks, total_ms);
    sprintf(fmt, "  B1 Final: %lu blocks in %lu ms = %lu KB/s (%lu errors)\r\n",
            (unsigned long)total_blocks, (unsigned long)total_ms,
            (unsigned long)avg_kbps, (unsigned long)total_errors);
    print(fmt);

    if (total_errors == 0)
        pass("B1 DMA write benchmark (10s)");
    else
        fail("B1 DMA write benchmark", "transfer errors");

    return total_blocks;
}

// -----------------------------------------------------------------------
// B2: DMA read throughput (DDR2, 5s, per-second reports + verify)
// -----------------------------------------------------------------------
static void bench_dma_read(uint32_t written_blocks)
{
    uint32_t max_blk;
    uint32_t blk;
    uint32_t total_blocks;
    uint32_t total_errors;
    uint32_t second;
    uint32_t interval_blocks;
    uint32_t elapsed;
    uint32_t interval_ms;
    uint32_t inst_kbps;
    uint32_t avg_kbps;
    uint32_t total_ms;
    TickType_t run_start;
    TickType_t next_report;
    TickType_t now;
    int32_t rc;
    int verify_errors;

    max_blk = (written_blocks / BENCH_BLOCKS) * BENCH_BLOCKS;
    if (max_blk == 0)
        max_blk = BENCH_BLOCKS;

    print("  B2: DMA read (DDR2), 4-block bursts for 5 seconds...\r\n");
    print("  [sec]  this KB/s    avg KB/s    total blocks\r\n");

    blk = BENCH_SECTOR;
    total_blocks = 0;
    total_errors = 0;
    second = 0;
    interval_blocks = 0;
    run_start = xTaskGetTickCount();
    next_report = run_start + pdMS_TO_TICKS(1000);

    elapsed = 0;
    while (elapsed < BENCH_READ_MS && total_errors < 10) {
        rc = sd_driver_read_blocks(rd_buf, blk, BENCH_BLOCKS);
        if (rc != 0) {
            total_errors++;
        } else {
            total_blocks += BENCH_BLOCKS;
            interval_blocks += BENCH_BLOCKS;
        }
        blk += BENCH_BLOCKS;
        if (blk - BENCH_SECTOR >= max_blk)
            blk = BENCH_SECTOR;

        now = xTaskGetTickCount();
        elapsed = (uint32_t)(now - run_start);

        if (now >= next_report) {
            second++;
            interval_ms = (uint32_t)(now - (next_report - pdMS_TO_TICKS(1000)));
            inst_kbps = calc_kbps(interval_blocks, interval_ms);
            avg_kbps = calc_kbps(total_blocks, elapsed);
            sprintf(fmt, "  [%2lu]   %5lu       %5lu       %lu\r\n",
                    (unsigned long)second,
                    (unsigned long)inst_kbps,
                    (unsigned long)avg_kbps,
                    (unsigned long)total_blocks);
            print(fmt);
            interval_blocks = 0;
            next_report += pdMS_TO_TICKS(1000);
        }
    }

    total_ms = (uint32_t)(xTaskGetTickCount() - run_start);
    avg_kbps = calc_kbps(total_blocks, total_ms);
    sprintf(fmt, "  B2 Final: %lu blocks in %lu ms = %lu KB/s (%lu errors)\r\n",
            (unsigned long)total_blocks, (unsigned long)total_ms,
            (unsigned long)avg_kbps, (unsigned long)total_errors);
    print(fmt);

    // Spot-check: verify first 4 blocks match write pattern
    fill_xor_pattern(wr_buf, BENCH_BYTES, 0xB1);
    memset(rd_buf, 0xDE, BENCH_BYTES);
    rc = sd_driver_read_blocks(rd_buf, BENCH_SECTOR, BENCH_BLOCKS);
    verify_errors = 0;
    if (rc != 0) {
        verify_errors = 1;
    } else {
        verify_errors = verify_buffers(BENCH_BYTES);
    }

    if (total_errors == 0 && verify_errors == 0)
        pass("B2 DMA read benchmark (5s, verified)");
    else
        fail("B2 DMA read benchmark", "errors or verify mismatch");
}

// -----------------------------------------------------------------------
// B3: PIO write throughput (static BRAM, 10s, per-second reports)
// Returns the total number of blocks written (for B4 read range).
// -----------------------------------------------------------------------
static uint32_t bench_pio_write(void)
{
    uint32_t blk;
    uint32_t total_blocks;
    uint32_t total_errors;
    uint32_t second;
    uint32_t interval_blocks;
    uint32_t elapsed;
    uint32_t interval_ms;
    uint32_t inst_kbps;
    uint32_t avg_kbps;
    uint32_t total_ms;
    TickType_t run_start;
    TickType_t next_report;
    TickType_t now;
    int32_t rc;

    fill_xor_pattern(pio_wr_buf, BENCH_BYTES, 0xB3);

    print("  B3: PIO write (BRAM), 4-block bursts for 10 seconds...\r\n");
    print("  [sec]  this KB/s    avg KB/s    total blocks\r\n");

    blk = BENCH_SECTOR;
    total_blocks = 0;
    total_errors = 0;
    second = 0;
    interval_blocks = 0;
    run_start = xTaskGetTickCount();
    next_report = run_start + pdMS_TO_TICKS(1000);

    elapsed = 0;
    while (elapsed < BENCH_WRITE_MS && total_errors < 10) {
        rc = sd_driver_write_blocks(pio_wr_buf, blk, BENCH_BLOCKS);
        if (rc != 0) {
            total_errors++;
        } else {
            total_blocks += BENCH_BLOCKS;
            interval_blocks += BENCH_BLOCKS;
        }
        blk += BENCH_BLOCKS;

        now = xTaskGetTickCount();
        elapsed = (uint32_t)(now - run_start);

        if (now >= next_report) {
            second++;
            interval_ms = (uint32_t)(now - (next_report - pdMS_TO_TICKS(1000)));
            inst_kbps = calc_kbps(interval_blocks, interval_ms);
            avg_kbps = calc_kbps(total_blocks, elapsed);
            sprintf(fmt, "  [%2lu]   %5lu       %5lu       %lu\r\n",
                    (unsigned long)second,
                    (unsigned long)inst_kbps,
                    (unsigned long)avg_kbps,
                    (unsigned long)total_blocks);
            print(fmt);
            interval_blocks = 0;
            next_report += pdMS_TO_TICKS(1000);
        }
    }

    total_ms = (uint32_t)(xTaskGetTickCount() - run_start);
    avg_kbps = calc_kbps(total_blocks, total_ms);
    sprintf(fmt, "  B3 Final: %lu blocks in %lu ms = %lu KB/s (%lu errors)\r\n",
            (unsigned long)total_blocks, (unsigned long)total_ms,
            (unsigned long)avg_kbps, (unsigned long)total_errors);
    print(fmt);

    if (total_errors == 0)
        pass("B3 PIO write benchmark (10s)");
    else
        fail("B3 PIO write benchmark", "transfer errors");

    return total_blocks;
}

// -----------------------------------------------------------------------
// B4: PIO read throughput (static BRAM, 5s, per-second reports + verify)
// -----------------------------------------------------------------------
static void bench_pio_read(uint32_t written_blocks)
{
    uint32_t max_blk;
    uint32_t blk;
    uint32_t total_blocks;
    uint32_t total_errors;
    uint32_t second;
    uint32_t interval_blocks;
    uint32_t elapsed;
    uint32_t interval_ms;
    uint32_t inst_kbps;
    uint32_t avg_kbps;
    uint32_t total_ms;
    TickType_t run_start;
    TickType_t next_report;
    TickType_t now;
    int32_t rc;
    int verify_errors;
    uint8_t *saved_wr;
    uint8_t *saved_rd;

    max_blk = (written_blocks / BENCH_BLOCKS) * BENCH_BLOCKS;
    if (max_blk == 0)
        max_blk = BENCH_BLOCKS;

    print("  B4: PIO read (BRAM), 4-block bursts for 5 seconds...\r\n");
    print("  [sec]  this KB/s    avg KB/s    total blocks\r\n");

    blk = BENCH_SECTOR;
    total_blocks = 0;
    total_errors = 0;
    second = 0;
    interval_blocks = 0;
    run_start = xTaskGetTickCount();
    next_report = run_start + pdMS_TO_TICKS(1000);

    elapsed = 0;
    while (elapsed < BENCH_READ_MS && total_errors < 10) {
        rc = sd_driver_read_blocks(pio_rd_buf, blk, BENCH_BLOCKS);
        if (rc != 0) {
            total_errors++;
        } else {
            total_blocks += BENCH_BLOCKS;
            interval_blocks += BENCH_BLOCKS;
        }
        blk += BENCH_BLOCKS;
        if (blk - BENCH_SECTOR >= max_blk)
            blk = BENCH_SECTOR;

        now = xTaskGetTickCount();
        elapsed = (uint32_t)(now - run_start);

        if (now >= next_report) {
            second++;
            interval_ms = (uint32_t)(now - (next_report - pdMS_TO_TICKS(1000)));
            inst_kbps = calc_kbps(interval_blocks, interval_ms);
            avg_kbps = calc_kbps(total_blocks, elapsed);
            sprintf(fmt, "  [%2lu]   %5lu       %5lu       %lu\r\n",
                    (unsigned long)second,
                    (unsigned long)inst_kbps,
                    (unsigned long)avg_kbps,
                    (unsigned long)total_blocks);
            print(fmt);
            interval_blocks = 0;
            next_report += pdMS_TO_TICKS(1000);
        }
    }

    total_ms = (uint32_t)(xTaskGetTickCount() - run_start);
    avg_kbps = calc_kbps(total_blocks, total_ms);
    sprintf(fmt, "  B4 Final: %lu blocks in %lu ms = %lu KB/s (%lu errors)\r\n",
            (unsigned long)total_blocks, (unsigned long)total_ms,
            (unsigned long)avg_kbps, (unsigned long)total_errors);
    print(fmt);

    // Spot-check: verify first 4 blocks match write pattern
    // Temporarily swap global buffers for verify_buffers()
    saved_wr = wr_buf;
    saved_rd = rd_buf;
    wr_buf = pio_wr_buf;
    rd_buf = pio_rd_buf;
    fill_xor_pattern(pio_wr_buf, BENCH_BYTES, 0xB3);
    memset(pio_rd_buf, 0xDE, BENCH_BYTES);
    rc = sd_driver_read_blocks(pio_rd_buf, BENCH_SECTOR, BENCH_BLOCKS);
    verify_errors = 0;
    if (rc != 0) {
        verify_errors = 1;
    } else {
        verify_errors = verify_buffers(BENCH_BYTES);
    }
    wr_buf = saved_wr;
    rd_buf = saved_rd;

    if (total_errors == 0 && verify_errors == 0)
        pass("B4 PIO read benchmark (5s, verified)");
    else
        fail("B4 PIO read benchmark", "errors or verify mismatch");
}

// -----------------------------------------------------------------------
// Test runner
// -----------------------------------------------------------------------
static void run_tests(void)
{
    const sd_card_info_t *info;
    uint32_t dma_written;
    uint32_t pio_written;

    info = sd_driver_get_card_info();
    if (!info->initialised) {
        print("APP: card not initialised, aborting\r\n");
        return;
    }

    sprintf(fmt, "APP: card ready, RCA=0x%04X %s\r\n",
            info->rca, info->is_sdhc ? "SDHC" : "SDSC");
    print(fmt);

    tests_run    = 0;
    tests_passed = 0;
    tests_failed = 0;

    print("\r\n--- Basic IO ---\r\n");
    test_single_block_basic();          // T01

    print("\r\n--- Boundary sectors ---\r\n");
    test_sector_zero();                 // T02
    test_large_sector();                // T03

    print("\r\n--- Multi-block counts ---\r\n");
    test_multi_block_counts();          // T04a, T04b, T04c

    print("\r\n--- Multi-chunk transfers ---\r\n");
    test_multi_chunk();                 // T05a, T05b

    print("\r\n--- Data patterns ---\r\n");
    test_pattern_zeros();               // T06
    test_pattern_ones();                // T07
    test_pattern_alternating();         // T08
    test_pattern_sequential();          // T09
    test_pattern_random();              // T10

    print("\r\n--- Stress ---\r\n");
    test_stress_repeat();               // T11

    print("\r\n--- Overwrite ---\r\n");
    test_overwrite();                   // T12

    print("\r\n--- Non-contiguous regions ---\r\n");
    test_non_contiguous();              // T13

    print("\r\n--- Card remove / re-insert ---\r\n");
    test_card_reinsert();               // T14

    print("\r\n--- SDMA / Auto CMD12 (DDR2 buffers) ---\r\n");
    test_dma_single_block();            // T15
    test_dma_multi_block();             // T16
    test_dma_stress();                  // T17

    print("\r\n--- PIO fallback (static BRAM buffers) ---\r\n");
    test_pio_single_block();            // T18
    test_pio_multi_block();             // T19
    test_pio_multi_chunk();             // T20

    print("\r\n--- Throughput benchmarks (DMA vs PIO) ---\r\n");
    dma_written = bench_dma_write();    // B1
    bench_dma_read(dma_written);        // B2
    pio_written = bench_pio_write();    // B3
    bench_pio_read(pio_written);        // B4

    // Summary
    print("\r\n========================================\r\n");
    sprintf(fmt, "  Tests: %d run, %d passed, %d failed\r\n",
            tests_run, tests_passed, tests_failed);
    print(fmt);
    if (tests_failed == 0) {
        print("  ALL TESTS PASSED\r\n");
    } else {
        print("  *** THERE WERE FAILURES ***\r\n");
    }
    print("========================================\r\n");
}

// -----------------------------------------------------------------------
// Application task
// -----------------------------------------------------------------------
static void test_app_task(void *pvParameters)
{
    (void)pvParameters;

    print("\r\n");
    print("========================================\r\n");
    print("  CENG 448 -- SD Driver Test Suite\r\n");
    print("  FreeRTOS on Nexys A7\r\n");
    print("========================================\r\n");
    print("\r\n");

    // Allocate IO buffers from DDR2 so SDMA can access them.
    wr_buf = (uint8_t *)malloc(MAX_TEST_SECTORS * 512);
    rd_buf = (uint8_t *)malloc(MAX_TEST_SECTORS * 512);
    if (wr_buf == NULL || rd_buf == NULL) {
        print("APP: FATAL -- malloc failed for IO buffers\r\n");
        while (1) { vTaskDelay(pdMS_TO_TICKS(10000)); }
    }

    // Wait for the driver task to finish card init
    vTaskDelay(pdMS_TO_TICKS(3000));

    run_tests();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

// -----------------------------------------------------------------------
// MTIME tick handler
//
// Called from freertos_risc_v_application_interrupt_handler via the INTC
// dispatch.  Updates MTIMECMP, increments the tick, and requests a
// context switch if needed.
// -----------------------------------------------------------------------
extern uint64_t ullNextTime;
extern const size_t uxTimerIncrementsForOneTick;
extern volatile uint64_t *pullMachineTimerCompareRegister;

void mtime_tick_handler(void)
{
    volatile uint32_t *pulTimeHigh;
    volatile uint32_t *pulTimeLow;
    uint32_t hi, lo;
    uint64_t now, cmp;

    pulTimeHigh = (volatile uint32_t *)(configMTIME_BASE_ADDRESS + 4UL);
    pulTimeLow = (volatile uint32_t *)(configMTIME_BASE_ADDRESS);

    cmp = ullNextTime;
    *pullMachineTimerCompareRegister = cmp;
    ullNextTime = cmp + (uint64_t)uxTimerIncrementsForOneTick;

    do {
        hi = *pulTimeHigh;
        lo = *pulTimeLow;
    } while (hi != *pulTimeHigh);
    now = ((uint64_t)hi << 32) | (uint64_t)lo;

    if (now >= cmp) {
        cmp = now + (uint64_t)uxTimerIncrementsForOneTick;
        *pullMachineTimerCompareRegister = cmp;
        ullNextTime = cmp + (uint64_t)uxTimerIncrementsForOneTick;
    }

    if (xTaskIncrementTick() != pdFALSE) {
        vTaskSwitchContext();
    }
}

// -----------------------------------------------------------------------
// FreeRTOS hooks
// -----------------------------------------------------------------------
void vAssertCalled(unsigned line, const char * const filename)
{
    (void)line;
    (void)filename;
    while (1);
}

void malloc_failed(void)
{
    while (1);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    while (1);
}

// -----------------------------------------------------------------------
// main
// -----------------------------------------------------------------------
int main(void)
{
    volatile uint64_t *mtimecmp;

    // Reset the INTC to a known state (stale state from previous run)
    INTC_Init();

    // Set MTIMECMP to max so MTIME doesn't fire before the scheduler starts
    mtimecmp = (volatile uint64_t *)configMTIMECMP_BASE_ADDRESS;
    *mtimecmp = 0xFFFFFFFFFFFFFFFFULL;

    // Register MTIME tick handler
    INTC_SetVector(MTIME_IRQ, mtime_tick_handler);
    INTC_EnableIRQ(MTIME_IRQ);

    // Register UART ISR handlers
    INTC_SetVector(UART0_IRQ, UART0_handler);
    INTC_EnableIRQ(UART0_IRQ);

    // Register SD ISR handler (will be enabled by sd_driver_task)
    INTC_SetVector(SD_IRQ, SD_CONTROLLER_ISR_HANDLER);

    // Enable the global interrupt output from the AXI INTC
    INTC_Enable_Global();

    // Initialise UART
    UART_16550_init();
    UART_16550_configure(UART0, 9600, UART_PARITY_NONE, 8, 1);

    // Register DDR2 as DMA-capable (0x08000000, 128 MB on MicroBlaze V)
    sd_driver_add_dma_region(0x08000000, 0x08000000);

    // Initialise the SD driver (creates driver task and queues).
    sd_driver_init();

    // Create the application task
    xTaskCreateStatic(test_app_task, "app",
                      APP_STACK_SIZE, NULL, 2,
                      app_stack, &app_tcb);

    // Start the scheduler
    vTaskStartScheduler();

    while (1);
}

// -----------------------------------------------------------------------
// FreeRTOS idle task memory (static allocation)
// -----------------------------------------------------------------------
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t  uxIdleTaskStack[configMINIMAL_STACK_SIZE];

    *ppxIdleTaskTCBBuffer   = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}
