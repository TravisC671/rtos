// sd_test.h -- SD Controller hardware test task for FreeRTOS
#ifndef SD_TEST_H
#define SD_TEST_H

#include <FreeRTOS.h>
#include <task.h>

// FreeRTOS task that runs through all SD controller hardware tests
// and reports results via UART_16550_write_string on UART0.
void sd_test_task(void *pvParameters);

#endif // SD_TEST_H
