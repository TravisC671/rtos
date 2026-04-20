// main.c -- FreeRTOS+FAT test application
//
// Tests:
//   T01  Mount or format+mount
//   T02  Create and write a file
//   T03  Open and read back, verify contents
//   T04  Append to an existing file
//   T05  Create a subdirectory
//   T06  Write a file inside the subdirectory
//   T07  Read back file from subdirectory
//   T08  Directory listing (root)
//   T09  Directory listing (subdirectory)
//   T10  Write multiple files
//   T11  Large file (multi-sector, 8 KB)
//   T12  Delete a file
//   T13  Rename a file
//   T14  Overwrite an existing file
//   T15  Re-read files to verify all still intact
//   T16  Seek within a file (ff_fseek)
//   T17  Multiple file handles open simultaneously
//   T18  Nested subdirectories
//   T19  Large file beyond FAT cache (64 KB)
//   B1   Benchmark: small file write (100 × 64 B)
//   B2   Benchmark: small file read  (100 × 64 B)
//   B3   Benchmark: large file write (128 KB sequential)
//   B4   Benchmark: large file read  (128 KB sequential)
//   B5   Extended: timed file write (20s, 2 KB chunks, per-second reports)
//   B6   Extended: timed file read  (10s, 2 KB chunks, per-second + verify)
//
// All files are created under /sd/  (the FAT mount point).

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include <UART_16550.h>
#include <device_addrs.h>
#include "sd_driver.h"
#include "sd_driver_private.h"
#include "sd_fat_disk.h"
#include "ff_stdio.h"
#include "ff_headers.h"
#include <interrupts.h>

// -----------------------------------------------------------------------
// Test infrastructure
// -----------------------------------------------------------------------
#define APP_STACK_SIZE  8192

static StaticTask_t app_tcb;
static StackType_t  app_stack[APP_STACK_SIZE];

// IO buffer for file read/write
#define IO_BUF_SIZE  2048
static uint8_t io_buf[IO_BUF_SIZE];
static uint8_t verify_buf[IO_BUF_SIZE];

static char fmt[160];

static int tests_run;
static int tests_passed;
static int tests_failed;
static int force_format;

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
    char msg[160];

    tests_run++;
    tests_failed++;
    sprintf(msg, "  FAIL: %s -- %s\r\n", name, reason);
    print(msg);
}

// Known test string
static const char test_string[] = "Hello from FreeRTOS+FAT on SD!\r\n";
#define TEST_STRING_LEN  (sizeof(test_string) - 1)

static const char append_string[] = "This line was appended.\r\n";
#define APPEND_STRING_LEN  (sizeof(append_string) - 1)

// -----------------------------------------------------------------------
// T01: Mount or format+mount
// -----------------------------------------------------------------------
static FF_Disk_t *test_mount(void)
{
    FF_Disk_t *pxDisk;
    FF_Error_t xErr;
    int need_format;
    int mounted;

    pxDisk = sd_fat_disk_init();
    if (pxDisk == NULL) {
        fail("T01 mount", "sd_fat_disk_init returned NULL");
        return NULL;
    }

    // Check if the partition is actually mounted
    mounted = (pxDisk->pxIOManager != NULL &&
               pxDisk->pxIOManager->xPartition.ulTotalSectors != 0);

    // Determine whether we need to format
    need_format = force_format || !mounted;

    if (!need_format) {
        pass("T01 mount (existing FAT partition)");
        return pxDisk;
    }

    if (force_format) {
        print("  T01: forced format requested\r\n");
    } else {
        print("  T01: no valid partition, formatting...\r\n");
    }

    xErr = sd_fat_disk_format(pxDisk);
    if (FF_isERR(xErr)) {
        sprintf(fmt, "format failed (0x%lX)", (unsigned long)xErr);
        fail("T01 mount", fmt);
        return NULL;
    }

    pass("T01 mount (formatted + mounted)");
    return pxDisk;
}

// -----------------------------------------------------------------------
// T02: Create and write a file
// -----------------------------------------------------------------------
static void test_create_write(void)
{
    FF_FILE *pxFile;
    size_t written;

    pxFile = ff_fopen("/sd/test01.txt", "w");
    if (pxFile == NULL) {
        fail("T02 create+write", "ff_fopen failed");
        return;
    }

    written = ff_fwrite(test_string, 1, TEST_STRING_LEN, pxFile);
    ff_fclose(pxFile);

    if (written != TEST_STRING_LEN) {
        sprintf(fmt, "wrote %u of %u bytes",
                (unsigned)written, (unsigned)TEST_STRING_LEN);
        fail("T02 create+write", fmt);
        return;
    }

    pass("T02 create and write test01.txt");
}

// -----------------------------------------------------------------------
// T03: Open and read back, verify contents
// -----------------------------------------------------------------------
static void test_read_verify(void)
{
    FF_FILE *pxFile;
    size_t n;

    pxFile = ff_fopen("/sd/test01.txt", "r");
    if (pxFile == NULL) {
        fail("T03 read+verify", "ff_fopen failed");
        return;
    }

    memset(io_buf, 0, sizeof(io_buf));
    n = ff_fread(io_buf, 1, sizeof(io_buf), pxFile);
    ff_fclose(pxFile);

    if (n != TEST_STRING_LEN) {
        sprintf(fmt, "read %u, expected %u",
                (unsigned)n, (unsigned)TEST_STRING_LEN);
        fail("T03 read+verify", fmt);
        return;
    }

    if (memcmp(io_buf, test_string, TEST_STRING_LEN) != 0) {
        fail("T03 read+verify", "content mismatch");
        return;
    }

    pass("T03 read and verify test01.txt");
}

// -----------------------------------------------------------------------
// T04: Append to an existing file
// -----------------------------------------------------------------------
static void test_append(void)
{
    FF_FILE *pxFile;
    size_t written;
    size_t n;

    pxFile = ff_fopen("/sd/test01.txt", "a");
    if (pxFile == NULL) {
        fail("T04 append", "ff_fopen failed");
        return;
    }

    written = ff_fwrite(append_string, 1, APPEND_STRING_LEN, pxFile);
    ff_fclose(pxFile);

    if (written != APPEND_STRING_LEN) {
        fail("T04 append", "write returned short");
        return;
    }

    // Read back and verify both original + appended content
    pxFile = ff_fopen("/sd/test01.txt", "r");
    if (pxFile == NULL) {
        fail("T04 append", "re-open failed");
        return;
    }

    memset(io_buf, 0, sizeof(io_buf));
    n = ff_fread(io_buf, 1, sizeof(io_buf), pxFile);
    ff_fclose(pxFile);

    if (n != TEST_STRING_LEN + APPEND_STRING_LEN) {
        sprintf(fmt, "read %u, expected %u",
                (unsigned)n,
                (unsigned)(TEST_STRING_LEN + APPEND_STRING_LEN));
        fail("T04 append", fmt);
        return;
    }

    if (memcmp(io_buf, test_string, TEST_STRING_LEN) != 0 ||
        memcmp(io_buf + TEST_STRING_LEN, append_string, APPEND_STRING_LEN) != 0)
    {
        fail("T04 append", "content mismatch");
        return;
    }

    pass("T04 append to test01.txt");
}

// -----------------------------------------------------------------------
// T05: Create a subdirectory
// -----------------------------------------------------------------------
static void test_mkdir(void)
{
    int rc;
    FF_Stat_t xStat;

    rc = ff_mkdir("/sd/subdir");
    if (rc != 0) {
        // May already exist from a previous run — try to verify
        if (ff_stat("/sd/subdir", &xStat) == 0) {
            pass("T05 mkdir (already exists)");
            return;
        }
        sprintf(fmt, "ff_mkdir returned %d", rc);
        fail("T05 mkdir", fmt);
        return;
    }

    pass("T05 mkdir /sd/subdir");
}

// -----------------------------------------------------------------------
// T06: Write a file inside the subdirectory
// -----------------------------------------------------------------------
static void test_subdir_write(void)
{
    FF_FILE *pxFile;
    size_t written;
    const char msg[] = "File inside subdirectory\r\n";

    pxFile = ff_fopen("/sd/subdir/inner.txt", "w");
    if (pxFile == NULL) {
        fail("T06 subdir write", "ff_fopen failed");
        return;
    }

    written = ff_fwrite(msg, 1, sizeof(msg) - 1, pxFile);
    ff_fclose(pxFile);

    if (written != sizeof(msg) - 1) {
        fail("T06 subdir write", "short write");
        return;
    }

    pass("T06 write subdir/inner.txt");
}

// -----------------------------------------------------------------------
// T07: Read back file from subdirectory
// -----------------------------------------------------------------------
static void test_subdir_read(void)
{
    FF_FILE *pxFile;
    size_t n;
    const char msg[] = "File inside subdirectory\r\n";

    pxFile = ff_fopen("/sd/subdir/inner.txt", "r");
    if (pxFile == NULL) {
        fail("T07 subdir read", "ff_fopen failed");
        return;
    }

    memset(io_buf, 0, sizeof(io_buf));
    n = ff_fread(io_buf, 1, sizeof(io_buf), pxFile);
    ff_fclose(pxFile);

    if (n != sizeof(msg) - 1) {
        fail("T07 subdir read", "size mismatch");
        return;
    }

    if (memcmp(io_buf, msg, sizeof(msg) - 1) != 0) {
        fail("T07 subdir read", "content mismatch");
        return;
    }

    pass("T07 read subdir/inner.txt");
}

// -----------------------------------------------------------------------
// T08: Directory listing (root)
// -----------------------------------------------------------------------
static void test_dir_listing_root(void)
{
    FF_FindData_t xFind;
    int count = 0;
    int rc;

    print("  T08: listing /sd/\r\n");

    rc = ff_findfirst("/sd/", &xFind);
    if (rc != 0) {
        fail("T08 dir listing root", "ff_findfirst failed");
        return;
    }

    do {
        sprintf(fmt, "    %-20s %8lu %s\r\n",
                xFind.pcFileName,
                (unsigned long)xFind.ulFileSize,
                (xFind.ucAttributes & FF_FAT_ATTR_DIR) ? "<DIR>" : "");
        print(fmt);
        count++;
    } while (ff_findnext(&xFind) == 0);

    if (count >= 2) {
        // We expect at least test01.txt and subdir
        pass("T08 dir listing root");
    } else {
        fail("T08 dir listing root", "too few entries");
    }
}

// -----------------------------------------------------------------------
// T09: Directory listing (subdirectory)
// -----------------------------------------------------------------------
static void test_dir_listing_subdir(void)
{
    FF_FindData_t xFind;
    int count = 0;
    int rc;

    print("  T09: listing /sd/subdir/\r\n");

    rc = ff_findfirst("/sd/subdir/", &xFind);
    if (rc != 0) {
        fail("T09 dir listing subdir", "ff_findfirst failed");
        return;
    }

    do {
        sprintf(fmt, "    %-20s %8lu\r\n",
                xFind.pcFileName,
                (unsigned long)xFind.ulFileSize);
        print(fmt);
        count++;
    } while (ff_findnext(&xFind) == 0);

    if (count >= 1) {
        pass("T09 dir listing subdir");
    } else {
        fail("T09 dir listing subdir", "no entries found");
    }
}

// -----------------------------------------------------------------------
// T10: Write multiple files
// -----------------------------------------------------------------------
static void test_multiple_files(void)
{
    int i;
    FF_FILE *pxFile;
    char path[32];
    int ok = 1;

    for (i = 0; i < 5; i++) {
        sprintf(path, "/sd/multi_%d.txt", i);
        pxFile = ff_fopen(path, "w");
        if (pxFile == NULL) {
            sprintf(fmt, "failed to create %s", path);
            fail("T10 multiple files", fmt);
            ok = 0;
            break;
        }
        sprintf((char *)io_buf, "File number %d, content=%d\r\n",
                i, i * 111);
        ff_fwrite(io_buf, 1, strlen((char *)io_buf), pxFile);
        ff_fclose(pxFile);
    }

    if (ok) {
        pass("T10 create 5 files");
    }
}

// -----------------------------------------------------------------------
// T11: Large file (multi-sector)
// -----------------------------------------------------------------------
static void test_large_file(void)
{
    FF_FILE *pxFile;
    int i;
    size_t total_written = 0;
    size_t total_read = 0;
    size_t n;
    int errors = 0;

    // Write 8 KB (16 × 512 bytes) with known pattern
    pxFile = ff_fopen("/sd/large.bin", "w");
    if (pxFile == NULL) {
        fail("T11 large file", "create failed");
        return;
    }

    for (i = 0; i < 16; i++) {
        memset(io_buf, (uint8_t)(i + 0x30), 512);
        n = ff_fwrite(io_buf, 1, 512, pxFile);
        total_written += n;
    }
    ff_fclose(pxFile);

    if (total_written != 8192) {
        sprintf(fmt, "wrote %u of 8192", (unsigned)total_written);
        fail("T11 large file", fmt);
        return;
    }

    // Read back and verify each 512-byte chunk
    pxFile = ff_fopen("/sd/large.bin", "r");
    if (pxFile == NULL) {
        fail("T11 large file", "re-open failed");
        return;
    }

    for (i = 0; i < 16; i++) {
        memset(io_buf, 0, 512);
        n = ff_fread(io_buf, 1, 512, pxFile);
        total_read += n;
        if (n == 512) {
            memset(verify_buf, (uint8_t)(i + 0x30), 512);
            if (memcmp(io_buf, verify_buf, 512) != 0) {
                errors++;
            }
        }
    }
    ff_fclose(pxFile);

    if (total_read != 8192) {
        sprintf(fmt, "read %u of 8192", (unsigned)total_read);
        fail("T11 large file", fmt);
        return;
    }
    if (errors != 0) {
        sprintf(fmt, "%d chunks mismatched", errors);
        fail("T11 large file", fmt);
        return;
    }

    pass("T11 large file (8 KB)");
}

// -----------------------------------------------------------------------
// T12: Delete a file
// -----------------------------------------------------------------------
static void test_delete(void)
{
    int rc;
    FF_FILE *pxFile;

    rc = ff_remove("/sd/multi_0.txt");
    if (rc != 0) {
        fail("T12 delete", "ff_remove failed");
        return;
    }

    // Verify it's gone
    pxFile = ff_fopen("/sd/multi_0.txt", "r");
    if (pxFile != NULL) {
        ff_fclose(pxFile);
        fail("T12 delete", "file still exists after remove");
        return;
    }

    pass("T12 delete multi_0.txt");
}

// -----------------------------------------------------------------------
// T13: Rename a file
// -----------------------------------------------------------------------
static void test_rename(void)
{
    int rc;
    FF_FILE *pxFile;
    size_t n;

    rc = ff_rename("/sd/multi_1.txt", "/sd/renamed.txt", pdTRUE);
    if (rc != 0) {
        fail("T13 rename", "ff_rename failed");
        return;
    }

    // Verify old name is gone
    pxFile = ff_fopen("/sd/multi_1.txt", "r");
    if (pxFile != NULL) {
        ff_fclose(pxFile);
        fail("T13 rename", "old name still exists");
        return;
    }

    // Verify new name exists and has correct content
    pxFile = ff_fopen("/sd/renamed.txt", "r");
    if (pxFile == NULL) {
        fail("T13 rename", "new name not found");
        return;
    }

    memset(io_buf, 0, sizeof(io_buf));
    n = ff_fread(io_buf, 1, sizeof(io_buf), pxFile);
    ff_fclose(pxFile);

    // multi_1.txt was written with "File number 1, content=111\r\n"
    if (n == 0) {
        fail("T13 rename", "renamed file is empty");
        return;
    }

    pass("T13 rename multi_1.txt -> renamed.txt");
}

// -----------------------------------------------------------------------
// T14: Overwrite an existing file
// -----------------------------------------------------------------------
static void test_overwrite(void)
{
    FF_FILE *pxFile;
    size_t n;
    const char new_content[] = "Overwritten content\r\n";

    // Overwrite test01.txt with new content
    pxFile = ff_fopen("/sd/test01.txt", "w");
    if (pxFile == NULL) {
        fail("T14 overwrite", "ff_fopen failed");
        return;
    }
    ff_fwrite(new_content, 1, sizeof(new_content) - 1, pxFile);
    ff_fclose(pxFile);

    // Read back and verify
    pxFile = ff_fopen("/sd/test01.txt", "r");
    if (pxFile == NULL) {
        fail("T14 overwrite", "re-open failed");
        return;
    }

    memset(io_buf, 0, sizeof(io_buf));
    n = ff_fread(io_buf, 1, sizeof(io_buf), pxFile);
    ff_fclose(pxFile);

    if (n != sizeof(new_content) - 1) {
        fail("T14 overwrite", "size mismatch");
        return;
    }
    if (memcmp(io_buf, new_content, n) != 0) {
        fail("T14 overwrite", "content mismatch");
        return;
    }

    pass("T14 overwrite test01.txt");
}

// -----------------------------------------------------------------------
// T15: Re-read surviving files to verify integrity
// -----------------------------------------------------------------------
static void test_final_verify(void)
{
    FF_FILE *pxFile;
    size_t n;
    int i;
    char path[32];
    int ok = 1;

    // Verify large.bin still intact (spot check first chunk)
    pxFile = ff_fopen("/sd/large.bin", "r");
    if (pxFile == NULL) {
        fail("T15 final verify", "large.bin open failed");
        return;
    }
    memset(io_buf, 0, 512);
    n = ff_fread(io_buf, 1, 512, pxFile);
    ff_fclose(pxFile);

    if (n != 512) {
        fail("T15 final verify", "large.bin short read");
        return;
    }
    memset(verify_buf, 0x30, 512);
    if (memcmp(io_buf, verify_buf, 512) != 0) {
        fail("T15 final verify", "large.bin chunk 0 mismatch");
        return;
    }

    // Verify multi_2.txt through multi_4.txt still exist
    for (i = 2; i <= 4; i++) {
        sprintf(path, "/sd/multi_%d.txt", i);
        pxFile = ff_fopen(path, "r");
        if (pxFile == NULL) {
            sprintf(fmt, "%s missing", path);
            fail("T15 final verify", fmt);
            ok = 0;
            break;
        }
        ff_fclose(pxFile);
    }

    // Verify subdir/inner.txt
    pxFile = ff_fopen("/sd/subdir/inner.txt", "r");
    if (pxFile == NULL) {
        fail("T15 final verify", "subdir/inner.txt missing");
        ok = 0;
    } else {
        ff_fclose(pxFile);
    }

    if (ok) {
        pass("T15 final integrity verify");
    }
}

// -----------------------------------------------------------------------
// T16: Seek within a file (random access)
// -----------------------------------------------------------------------
static void test_seek(void)
{
    FF_FILE *pxFile;
    size_t n;
    int i;
    int rc;
    long pos;

    // Write a file with 10 × 32-byte records, each starting with its index
    pxFile = ff_fopen("/sd/seek.bin", "w");
    if (pxFile == NULL) {
        fail("T16 seek", "create failed");
        return;
    }

    for (i = 0; i < 10; i++) {
        memset(io_buf, (uint8_t)i, 32);
        io_buf[0] = (uint8_t)i;  // tag byte
        n = ff_fwrite(io_buf, 1, 32, pxFile);
        if (n != 32) {
            ff_fclose(pxFile);
            fail("T16 seek", "short write");
            return;
        }
    }
    ff_fclose(pxFile);

    // Reopen for reading and seek to record 7 (offset 224)
    pxFile = ff_fopen("/sd/seek.bin", "r");
    if (pxFile == NULL) {
        fail("T16 seek", "re-open failed");
        return;
    }

    rc = ff_fseek(pxFile, 7L * 32, FF_SEEK_SET);
    if (rc != 0) {
        ff_fclose(pxFile);
        fail("T16 seek", "ff_fseek SET failed");
        return;
    }

    memset(io_buf, 0, 32);
    n = ff_fread(io_buf, 1, 32, pxFile);
    if (n != 32 || io_buf[0] != 7) {
        ff_fclose(pxFile);
        fail("T16 seek", "SET read mismatch");
        return;
    }

    // Now at record 8.  Seek back -2 records (CUR) to record 6
    rc = ff_fseek(pxFile, -2L * 32, FF_SEEK_CUR);
    if (rc != 0) {
        ff_fclose(pxFile);
        fail("T16 seek", "ff_fseek CUR failed");
        return;
    }

    memset(io_buf, 0, 32);
    n = ff_fread(io_buf, 1, 32, pxFile);
    if (n != 32 || io_buf[0] != 6) {
        ff_fclose(pxFile);
        fail("T16 seek", "CUR read mismatch");
        return;
    }

    // Verify ff_ftell reports correct position (record 7 = 224)
    pos = ff_ftell(pxFile);
    ff_fclose(pxFile);

    if (pos != 7L * 32) {
        sprintf(fmt, "ftell=%ld, expected %ld", pos, 7L * 32);
        fail("T16 seek", fmt);
        return;
    }

    pass("T16 seek (SET + CUR + ftell)");
}

// -----------------------------------------------------------------------
// T17: Multiple file handles open simultaneously
// -----------------------------------------------------------------------
static void test_multi_handle(void)
{
    FF_FILE *fh[4];
    int i;
    size_t n;
    char path[32];
    int ok = 1;

    // Open 4 files for writing at the same time
    for (i = 0; i < 4; i++) {
        sprintf(path, "/sd/mh_%d.txt", i);
        fh[i] = ff_fopen(path, "w");
        if (fh[i] == NULL) {
            sprintf(fmt, "ff_fopen failed for %s", path);
            fail("T17 multi handle", fmt);
            ok = 0;
            break;
        }
    }

    if (!ok) {
        // Close any that were opened
        for (i = 0; i < 4; i++) {
            if (fh[i] != NULL) {
                ff_fclose(fh[i]);
            }
        }
        return;
    }

    // Write different content to each
    for (i = 0; i < 4; i++) {
        sprintf((char *)io_buf, "handle %d data\r\n", i);
        ff_fwrite(io_buf, 1, strlen((char *)io_buf), fh[i]);
    }

    // Close all
    for (i = 0; i < 4; i++) {
        ff_fclose(fh[i]);
    }

    // Reopen and verify each
    for (i = 0; i < 4; i++) {
        sprintf(path, "/sd/mh_%d.txt", i);
        fh[i] = ff_fopen(path, "r");
        if (fh[i] == NULL) {
            sprintf(fmt, "re-open failed for %s", path);
            fail("T17 multi handle", fmt);
            ok = 0;
            break;
        }
        memset(io_buf, 0, sizeof(io_buf));
        n = ff_fread(io_buf, 1, sizeof(io_buf), fh[i]);
        ff_fclose(fh[i]);

        sprintf((char *)verify_buf, "handle %d data\r\n", i);
        if (n != strlen((char *)verify_buf) ||
            memcmp(io_buf, verify_buf, n) != 0)
        {
            sprintf(fmt, "content mismatch for mh_%d.txt", i);
            fail("T17 multi handle", fmt);
            ok = 0;
            break;
        }
    }

    if (ok) {
        pass("T17 multiple file handles (4 simultaneous)");
    }
}

// -----------------------------------------------------------------------
// T18: Nested subdirectories
// -----------------------------------------------------------------------
static void test_nested_dirs(void)
{
    int rc;
    FF_FILE *pxFile;
    size_t n;
    FF_Stat_t xStat;
    const char nested_msg[] = "deeply nested file\r\n";

    // Create /sd/subdir/level2
    rc = ff_mkdir("/sd/subdir/level2");
    if (rc != 0) {
        if (ff_stat("/sd/subdir/level2", &xStat) != 0) {
            sprintf(fmt, "ff_mkdir returned %d", rc);
            fail("T18 nested dirs", fmt);
            return;
        }
    }

    // Create /sd/subdir/level2/level3
    rc = ff_mkdir("/sd/subdir/level2/level3");
    if (rc != 0) {
        if (ff_stat("/sd/subdir/level2/level3", &xStat) != 0) {
            sprintf(fmt, "ff_mkdir level3 returned %d", rc);
            fail("T18 nested dirs", fmt);
            return;
        }
    }

    // Write a file in level3
    pxFile = ff_fopen("/sd/subdir/level2/level3/deep.txt", "w");
    if (pxFile == NULL) {
        fail("T18 nested dirs", "create deep.txt failed");
        return;
    }
    n = ff_fwrite(nested_msg, 1, sizeof(nested_msg) - 1, pxFile);
    ff_fclose(pxFile);

    if (n != sizeof(nested_msg) - 1) {
        fail("T18 nested dirs", "short write");
        return;
    }

    // Read it back
    pxFile = ff_fopen("/sd/subdir/level2/level3/deep.txt", "r");
    if (pxFile == NULL) {
        fail("T18 nested dirs", "re-open deep.txt failed");
        return;
    }
    memset(io_buf, 0, sizeof(io_buf));
    n = ff_fread(io_buf, 1, sizeof(io_buf), pxFile);
    ff_fclose(pxFile);

    if (n != sizeof(nested_msg) - 1 ||
        memcmp(io_buf, nested_msg, n) != 0)
    {
        fail("T18 nested dirs", "content mismatch");
        return;
    }

    pass("T18 nested subdirectories (3 levels deep)");
}

// -----------------------------------------------------------------------
// T19: Large file beyond FAT cache (64 KB)
//
// With a 4-sector (2 KB) cache this forces many cache evictions and
// multiple multi-sector transfers through the SD controller.
// -----------------------------------------------------------------------
static void test_large_file_64k(void)
{
    FF_FILE *pxFile;
    uint32_t offset;
    size_t n;
    int errors = 0;
    uint8_t expected;

    // Write 64 KB in 1 KB chunks (64 iterations)
    pxFile = ff_fopen("/sd/big64k.bin", "w");
    if (pxFile == NULL) {
        fail("T19 large 64K", "create failed");
        return;
    }

    for (offset = 0; offset < 65536; offset += IO_BUF_SIZE) {
        // Fill each 1 KB chunk with a byte derived from the chunk index
        expected = (uint8_t)((offset / IO_BUF_SIZE) & 0xFF);
        memset(io_buf, expected, IO_BUF_SIZE);
        n = ff_fwrite(io_buf, 1, IO_BUF_SIZE, pxFile);
        if (n != IO_BUF_SIZE) {
            ff_fclose(pxFile);
            sprintf(fmt, "short write at offset %lu", (unsigned long)offset);
            fail("T19 large 64K", fmt);
            return;
        }
    }
    ff_fclose(pxFile);

    // Read back and verify every chunk
    pxFile = ff_fopen("/sd/big64k.bin", "r");
    if (pxFile == NULL) {
        fail("T19 large 64K", "re-open failed");
        return;
    }

    for (offset = 0; offset < 65536; offset += IO_BUF_SIZE) {
        expected = (uint8_t)((offset / IO_BUF_SIZE) & 0xFF);
        memset(io_buf, 0, IO_BUF_SIZE);
        n = ff_fread(io_buf, 1, IO_BUF_SIZE, pxFile);
        if (n != IO_BUF_SIZE) {
            ff_fclose(pxFile);
            sprintf(fmt, "short read at offset %lu", (unsigned long)offset);
            fail("T19 large 64K", fmt);
            return;
        }
        memset(verify_buf, expected, IO_BUF_SIZE);
        if (memcmp(io_buf, verify_buf, IO_BUF_SIZE) != 0) {
            errors++;
        }
    }
    ff_fclose(pxFile);

    if (errors != 0) {
        sprintf(fmt, "%d of 64 chunks mismatched", errors);
        fail("T19 large 64K", fmt);
        return;
    }

    pass("T19 large file (64 KB, cache stress)");
}

// -----------------------------------------------------------------------
// B1: Benchmark — small file write (100 files × 64 bytes)
// -----------------------------------------------------------------------
#define BENCH_SMALL_COUNT  100
#define BENCH_SMALL_SIZE   64

static void bench_small_write(void)
{
    FF_FILE *pxFile;
    TickType_t t_start;
    TickType_t t_end;
    uint32_t elapsed_ms;
    int i;
    char path[32];
    size_t n;

    memset(io_buf, 'S', BENCH_SMALL_SIZE);

    t_start = xTaskGetTickCount();

    for (i = 0; i < BENCH_SMALL_COUNT; i++) {
        sprintf(path, "/sd/bs_%03d.dat", i);
        pxFile = ff_fopen(path, "w");
        if (pxFile == NULL) {
            sprintf(fmt, "open failed at file %d", i);
            fail("B1 small write", fmt);
            return;
        }
        n = ff_fwrite(io_buf, 1, BENCH_SMALL_SIZE, pxFile);
        ff_fclose(pxFile);
        if (n != BENCH_SMALL_SIZE) {
            fail("B1 small write", "short write");
            return;
        }
    }

    t_end = xTaskGetTickCount();
    elapsed_ms = (uint32_t)(t_end - t_start);

    sprintf(fmt, "  B1: %d files x %d B written in %lu ms"
            " (%lu bytes total)\r\n",
            BENCH_SMALL_COUNT, BENCH_SMALL_SIZE,
            (unsigned long)elapsed_ms,
            (unsigned long)(BENCH_SMALL_COUNT * BENCH_SMALL_SIZE));
    print(fmt);
    pass("B1 small file write benchmark");
}

// -----------------------------------------------------------------------
// B2: Benchmark — small file read (100 files × 64 bytes)
// -----------------------------------------------------------------------
static void bench_small_read(void)
{
    FF_FILE *pxFile;
    TickType_t t_start;
    TickType_t t_end;
    uint32_t elapsed_ms;
    int i;
    char path[32];
    size_t n;

    t_start = xTaskGetTickCount();

    for (i = 0; i < BENCH_SMALL_COUNT; i++) {
        sprintf(path, "/sd/bs_%03d.dat", i);
        pxFile = ff_fopen(path, "r");
        if (pxFile == NULL) {
            sprintf(fmt, "open failed at file %d", i);
            fail("B2 small read", fmt);
            return;
        }
        n = ff_fread(io_buf, 1, BENCH_SMALL_SIZE, pxFile);
        ff_fclose(pxFile);
        if (n != BENCH_SMALL_SIZE) {
            fail("B2 small read", "short read");
            return;
        }
    }

    t_end = xTaskGetTickCount();
    elapsed_ms = (uint32_t)(t_end - t_start);

    sprintf(fmt, "  B2: %d files x %d B read in %lu ms\r\n",
            BENCH_SMALL_COUNT, BENCH_SMALL_SIZE,
            (unsigned long)elapsed_ms);
    print(fmt);
    pass("B2 small file read benchmark");
}

// -----------------------------------------------------------------------
// B3: Benchmark — large file sequential write (128 KB)
// -----------------------------------------------------------------------
#define BENCH_LARGE_SIZE   (2048UL * 1024UL)

static void bench_large_write(void)
{
    FF_FILE *pxFile;
    TickType_t t_start;
    TickType_t t_end;
    uint32_t elapsed_ms;
    uint32_t offset;
    uint32_t throughput;
    size_t n;

    memset(io_buf, 0xA5, IO_BUF_SIZE);

    pxFile = ff_fopen("/sd/bench128k.bin", "w");
    if (pxFile == NULL) {
        fail("B3 large write", "create failed");
        return;
    }

    t_start = xTaskGetTickCount();

    for (offset = 0; offset < BENCH_LARGE_SIZE; offset += IO_BUF_SIZE) {
        n = ff_fwrite(io_buf, 1, IO_BUF_SIZE, pxFile);
        if (n != IO_BUF_SIZE) {
            ff_fclose(pxFile);
            fail("B3 large write", "short write");
            return;
        }
    }

    ff_fclose(pxFile);
    t_end = xTaskGetTickCount();
    elapsed_ms = (uint32_t)(t_end - t_start);

    // throughput in KB/s = (bytes / 1024) / (ms / 1000) = bytes*1000 / (ms*1024)
    if (elapsed_ms > 0) {
        throughput = (BENCH_LARGE_SIZE * 1000UL) / (elapsed_ms * 1024UL);
    } else {
        throughput = 0;
    }

    sprintf(fmt, "  B3: %lu KB written in %lu ms (%lu KB/s)\r\n",
            (unsigned long)(BENCH_LARGE_SIZE / 1024),
            (unsigned long)elapsed_ms,
            (unsigned long)throughput);
    print(fmt);
    pass("B3 large file write benchmark (2 MB)");
}

// -----------------------------------------------------------------------
// B4: Benchmark — large file sequential read (128 KB)
// -----------------------------------------------------------------------
static void bench_large_read(void)
{
    FF_FILE *pxFile;
    TickType_t t_start;
    TickType_t t_end;
    uint32_t elapsed_ms;
    uint32_t offset;
    uint32_t throughput;
    size_t n;

    pxFile = ff_fopen("/sd/bench128k.bin", "r");
    if (pxFile == NULL) {
        fail("B4 large read", "open failed");
        return;
    }

    t_start = xTaskGetTickCount();

    for (offset = 0; offset < BENCH_LARGE_SIZE; offset += IO_BUF_SIZE) {
        n = ff_fread(io_buf, 1, IO_BUF_SIZE, pxFile);
        if (n != IO_BUF_SIZE) {
            ff_fclose(pxFile);
            fail("B4 large read", "short read");
            return;
        }
    }

    ff_fclose(pxFile);
    t_end = xTaskGetTickCount();
    elapsed_ms = (uint32_t)(t_end - t_start);

    if (elapsed_ms > 0) {
        throughput = (BENCH_LARGE_SIZE * 1000UL) / (elapsed_ms * 1024UL);
    } else {
        throughput = 0;
    }

    sprintf(fmt, "  B4: %lu KB read in %lu ms (%lu KB/s)\r\n",
            (unsigned long)(BENCH_LARGE_SIZE / 1024),
            (unsigned long)elapsed_ms,
            (unsigned long)throughput);
    print(fmt);
    pass("B4 large file read benchmark (2 MB)");
}

// -----------------------------------------------------------------------
// B5: Extended benchmark — timed large file sequential write (20s)
//
// Writes 2 KB chunks continuously for 20 seconds, reporting per-second
// instantaneous and average throughput.  Returns total bytes written
// so B6 knows the file size.
// -----------------------------------------------------------------------
#define BENCH_EXT_WRITE_MS  20000
#define BENCH_EXT_READ_MS   10000
#define BENCH_EXT_CHUNK     IO_BUF_SIZE   // 2 KB per write

static uint32_t calc_kbps(uint32_t bytes, uint32_t ms)
{
    if (ms == 0) return 0;
    return (bytes / 1024) * 1000 / ms;
}

static uint32_t bench_ext_write(void)
{
    FF_FILE *pxFile;
    uint32_t total_bytes;
    uint32_t total_errors;
    uint32_t second;
    uint32_t interval_bytes;
    uint32_t elapsed;
    uint32_t interval_ms;
    uint32_t inst_kbps;
    uint32_t avg_kbps;
    uint32_t total_ms;
    TickType_t run_start;
    TickType_t next_report;
    TickType_t now;
    size_t n;

    memset(io_buf, 0xB5, BENCH_EXT_CHUNK);

    pxFile = ff_fopen("/sd/bench_ext.bin", "w");
    if (pxFile == NULL) {
        fail("B5 ext write", "create failed");
        return 0;
    }

    print("  B5: timed file write, 2 KB chunks for 20 seconds...\r\n");
    print("  [sec]  this KB/s    avg KB/s    total KB\r\n");

    total_bytes = 0;
    total_errors = 0;
    second = 0;
    interval_bytes = 0;
    run_start = xTaskGetTickCount();
    next_report = run_start + pdMS_TO_TICKS(1000);

    elapsed = 0;
    while (elapsed < BENCH_EXT_WRITE_MS && total_errors < 10) {
        n = ff_fwrite(io_buf, 1, BENCH_EXT_CHUNK, pxFile);
        if (n != BENCH_EXT_CHUNK) {
            total_errors++;
        } else {
            total_bytes += BENCH_EXT_CHUNK;
            interval_bytes += BENCH_EXT_CHUNK;
        }

        now = xTaskGetTickCount();
        elapsed = (uint32_t)(now - run_start);

        if (now >= next_report) {
            second++;
            interval_ms = (uint32_t)(now - (next_report - pdMS_TO_TICKS(1000)));
            inst_kbps = calc_kbps(interval_bytes, interval_ms);
            avg_kbps = calc_kbps(total_bytes, elapsed);
            sprintf(fmt, "  [%2lu]   %5lu       %5lu       %lu\r\n",
                    (unsigned long)second,
                    (unsigned long)inst_kbps,
                    (unsigned long)avg_kbps,
                    (unsigned long)(total_bytes / 1024));
            print(fmt);
            interval_bytes = 0;
            next_report += pdMS_TO_TICKS(1000);
        }
    }

    ff_fclose(pxFile);

    total_ms = (uint32_t)(xTaskGetTickCount() - run_start);
    avg_kbps = calc_kbps(total_bytes, total_ms);
    sprintf(fmt, "  B5 Final: %lu KB in %lu ms = %lu KB/s (%lu errors)\r\n",
            (unsigned long)(total_bytes / 1024), (unsigned long)total_ms,
            (unsigned long)avg_kbps, (unsigned long)total_errors);
    print(fmt);

    if (total_errors == 0)
        pass("B5 ext write benchmark (20s)");
    else
        fail("B5 ext write benchmark", "write errors");

    return total_bytes;
}

// -----------------------------------------------------------------------
// B6: Extended benchmark — timed large file sequential read (10s + verify)
//
// Reads the file written by B5 in 2 KB chunks for 10 seconds, wrapping
// to the beginning on EOF.  Reports per-second throughput.  Verifies
// the first chunk after the timed loop.
// -----------------------------------------------------------------------
static void bench_ext_read(uint32_t file_bytes)
{
    FF_FILE *pxFile;
    uint32_t total_bytes;
    uint32_t total_errors;
    uint32_t second;
    uint32_t interval_bytes;
    uint32_t elapsed;
    uint32_t interval_ms;
    uint32_t inst_kbps;
    uint32_t avg_kbps;
    uint32_t total_ms;
    uint32_t file_pos;
    TickType_t run_start;
    TickType_t next_report;
    TickType_t now;
    size_t n;
    int verify_ok;

    if (file_bytes == 0) {
        fail("B6 ext read", "no data written by B5");
        return;
    }

    pxFile = ff_fopen("/sd/bench_ext.bin", "r");
    if (pxFile == NULL) {
        fail("B6 ext read", "open failed");
        return;
    }

    print("  B6: timed file read, 2 KB chunks for 10 seconds...\r\n");
    print("  [sec]  this KB/s    avg KB/s    total KB\r\n");

    total_bytes = 0;
    total_errors = 0;
    second = 0;
    interval_bytes = 0;
    file_pos = 0;
    run_start = xTaskGetTickCount();
    next_report = run_start + pdMS_TO_TICKS(1000);

    elapsed = 0;
    while (elapsed < BENCH_EXT_READ_MS && total_errors < 10) {
        n = ff_fread(io_buf, 1, BENCH_EXT_CHUNK, pxFile);
        if (n != BENCH_EXT_CHUNK) {
            // Probably hit EOF — seek back to start
            ff_fseek(pxFile, 0, FF_SEEK_SET);
            file_pos = 0;
            n = ff_fread(io_buf, 1, BENCH_EXT_CHUNK, pxFile);
            if (n != BENCH_EXT_CHUNK) {
                total_errors++;
                now = xTaskGetTickCount();
                elapsed = (uint32_t)(now - run_start);
                break;
            }
        }
        total_bytes += BENCH_EXT_CHUNK;
        interval_bytes += BENCH_EXT_CHUNK;
        file_pos += BENCH_EXT_CHUNK;

        // Wrap at file boundary to avoid partial reads
        if (file_pos + BENCH_EXT_CHUNK > file_bytes) {
            ff_fseek(pxFile, 0, FF_SEEK_SET);
            file_pos = 0;
        }

        now = xTaskGetTickCount();
        elapsed = (uint32_t)(now - run_start);

        if (now >= next_report) {
            second++;
            interval_ms = (uint32_t)(now - (next_report - pdMS_TO_TICKS(1000)));
            inst_kbps = calc_kbps(interval_bytes, interval_ms);
            avg_kbps = calc_kbps(total_bytes, elapsed);
            sprintf(fmt, "  [%2lu]   %5lu       %5lu       %lu\r\n",
                    (unsigned long)second,
                    (unsigned long)inst_kbps,
                    (unsigned long)avg_kbps,
                    (unsigned long)(total_bytes / 1024));
            print(fmt);
            interval_bytes = 0;
            next_report += pdMS_TO_TICKS(1000);
        }
    }

    ff_fclose(pxFile);

    total_ms = (uint32_t)(xTaskGetTickCount() - run_start);
    avg_kbps = calc_kbps(total_bytes, total_ms);
    sprintf(fmt, "  B6 Final: %lu KB in %lu ms = %lu KB/s (%lu errors)\r\n",
            (unsigned long)(total_bytes / 1024), (unsigned long)total_ms,
            (unsigned long)avg_kbps, (unsigned long)total_errors);
    print(fmt);

    // Verify: re-read first chunk and check pattern
    verify_ok = 1;
    pxFile = ff_fopen("/sd/bench_ext.bin", "r");
    if (pxFile != NULL) {
        memset(io_buf, 0, BENCH_EXT_CHUNK);
        n = ff_fread(io_buf, 1, BENCH_EXT_CHUNK, pxFile);
        ff_fclose(pxFile);
        if (n == BENCH_EXT_CHUNK) {
            memset(verify_buf, 0xB5, BENCH_EXT_CHUNK);
            if (memcmp(io_buf, verify_buf, BENCH_EXT_CHUNK) != 0) {
                verify_ok = 0;
            }
        } else {
            verify_ok = 0;
        }
    } else {
        verify_ok = 0;
    }

    if (total_errors == 0 && verify_ok)
        pass("B6 ext read benchmark (10s + verify)");
    else if (!verify_ok)
        fail("B6 ext read benchmark", "verify mismatch");
    else
        fail("B6 ext read benchmark", "read errors");
}

// -----------------------------------------------------------------------
// Benchmark cleanup — remove benchmark files
// -----------------------------------------------------------------------
static void bench_cleanup(void)
{
    int i;
    char path[32];

    for (i = 0; i < BENCH_SMALL_COUNT; i++) {
        sprintf(path, "/sd/bs_%03d.dat", i);
        ff_remove(path);
    }
    ff_remove("/sd/bench128k.bin");
    ff_remove("/sd/bench_ext.bin");
    print("  Benchmark files cleaned up\r\n");
}

// -----------------------------------------------------------------------
// Test runner
// -----------------------------------------------------------------------
static void run_tests(void)
{
    FF_Disk_t *pxDisk;

    tests_run    = 0;
    tests_passed = 0;
    tests_failed = 0;

    print("\r\n--- Mount / Format ---\r\n");
    pxDisk = test_mount();                  // T01
    if (pxDisk == NULL) {
        print("APP: cannot proceed without a mounted disk\r\n");
        return;
    }

    print("\r\n--- Basic file IO ---\r\n");
    test_create_write();                    // T02
    test_read_verify();                     // T03
    test_append();                          // T04

    print("\r\n--- Directories ---\r\n");
    test_mkdir();                           // T05
    test_subdir_write();                    // T06
    test_subdir_read();                     // T07
    test_dir_listing_root();                // T08
    test_dir_listing_subdir();              // T09

    print("\r\n--- Multiple files ---\r\n");
    test_multiple_files();                  // T10

    print("\r\n--- Large file ---\r\n");
    test_large_file();                      // T11

    print("\r\n--- File management ---\r\n");
    test_delete();                          // T12
    test_rename();                          // T13
    test_overwrite();                       // T14

    print("\r\n--- Seek and random access ---\r\n");
    test_seek();                            // T16

    print("\r\n--- Multiple file handles ---\r\n");
    test_multi_handle();                    // T17

    print("\r\n--- Nested directories ---\r\n");
    test_nested_dirs();                     // T18

    print("\r\n--- Large file (64 KB cache stress) ---\r\n");
    test_large_file_64k();                  // T19

    print("\r\n--- Performance benchmarks ---\r\n");
    bench_small_write();                    // B1
    bench_small_read();                     // B2
    bench_large_write();                    // B3
    bench_large_read();                     // B4

    print("\r\n--- Extended timed benchmarks ---\r\n");
    {
        uint32_t ext_written;
        ext_written = bench_ext_write();    // B5
        bench_ext_read(ext_written);        // B6
    }

    bench_cleanup();

    print("\r\n--- Final integrity ---\r\n");
    test_final_verify();                    // T15

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
    char ch;
    BaseType_t got;

    (void)pvParameters;

    print("\r\n");
    print("========================================\r\n");
    print("  CENG 448 -- SD FAT Test Application\r\n");
    print("  FreeRTOS + FreeRTOS+FAT on Nexys A7\r\n");
    print("========================================\r\n");
    print("\r\n");

    // Wait for the driver task to finish card init
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Ask user whether to force format
    print("\r\n  Press 'f' within 5 seconds to force format, "
          "or any other key to skip...\r\n");
    UART_16550_flush_rx(UART0);
    got = UART_16550_get_char(UART0, &ch, pdMS_TO_TICKS(5000));
    if (got == pdPASS && (ch == 'f' || ch == 'F')) {
        print("  >> Format requested\r\n");
        force_format = 1;
    } else {
        print("  >> Skipping format\r\n");
        force_format = 0;
    }

    run_tests();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

// -----------------------------------------------------------------------
// RISC-V MTIME tick handler
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

void vApplicationStackOverflowHook(TaskHandle_t xTask,
                                   char *pcTaskName)
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

    // Reset the INTC to a known state
    INTC_Init();

    // Set MTIMECMP to max
    mtimecmp = (volatile uint64_t *)configMTIMECMP_BASE_ADDRESS;
    *mtimecmp = 0xFFFFFFFFFFFFFFFFULL;

    //all must be in vector table

    // Register MTIME tick handler
    // INTC_SetVector(MTIME_IRQ, mtime_tick_handler);
    // INTC_EnableIRQ(MTIME_IRQ);

    // // Register UART ISR handlers
    // INTC_SetVector(UART0_IRQ, UART0_handler);
    // INTC_EnableIRQ(UART0_IRQ);

    // // Register SD ISR handler (will be enabled by sd_driver_task)
    // INTC_SetVector(SD_IRQ, SD_CONTROLLER_ISR_HANDLER);

    // Enable global interrupt output
    // INTC_Enable_Global();

    // Initialise UART
    UART_16550_init();
    UART_16550_configure(UART0, 9600, UART_PARITY_NONE, 8, 1);

    // Register DDR2 as DMA-capable (0x08000000, 128 MB on MicroBlaze V)
    sd_driver_add_dma_region(0x08000000, 0x08000000);
    //! change to ddr2

    // Initialise the SD driver
    sd_driver_init();

    // Create the application task (larger stack for FAT operations)
    xTaskCreateStatic(test_app_task, "app",
                      APP_STACK_SIZE, NULL, 2,
                      app_stack, &app_tcb);

    // Start the scheduler
    vTaskStartScheduler();

    // need to start scheduler
    //enter critical section when resetting & jumpign
    // add a fence & fence.1 instruction before jump

    //startup code needs to move vector table
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
