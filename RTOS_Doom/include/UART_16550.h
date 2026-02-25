#ifndef UART_16550_h
#define UART_16550_h

#include <FreeRTOS.h>

#define UART0 0
#define UART1 1

// This file defines the API for the 16550 UART driver.

// uart is driven by the system clock
#define UART_16550_clk 50000000

#define UART_PARITY_NONE 0
#define UART_PARITY_EVEN 1
#define UART_PARITY_ODD  2

// Initialize the 16550 UART driver and all 16550 UART devices. This
// should be called once during the OS initialisation phase of
// bootup/reset.
void UART_16550_init();

/* Set the baud, rate, parity, bits per frame, and number of stop bits
 * for the given UART.
 *
 * - baud can be anything that the hardware can support.
 * - parity should be UART_16550_PARITY_EVEN, UART_16550_PARITY_ODD, or
 *     UART_16550_PARITY_NONE
 * - bits can be between 5 and 8
 * - stop_bits can be 1 or 2
 *
 * In most RTOS use cases, this should be called only once for each
 * UART, in the application initialization phase of bootup/reset.  If
 * only a single task will use the UART, then this can be called in the
 * startup code of that task, before it enters its main loop.
 */
void UART_16550_configure(int UART,int baud,int parity,int bits,int stop_bits);

/************* Functions that tasks can use *************************/

/* Lock the given UART transmitter so that no other task can write to
   it. Returns pdPASS if the lock is acquired. */
BaseType_t UART_16550_tx_lock(int UART,
			      TickType_t xTicksToWait);

/* Unlock the given UART transmitter so that other tasks can write to
   it. */ 
void UART_16550_tx_unlock(int UART);

/* Try to write a character to the UART. */
BaseType_t UART_16550_put_char(int UART,
			       char c,
			       TickType_t xTicksToWait);

/* Write a string to the UART. */
BaseType_t UART_16550_write_string(int UART,
				   char *s,
				   TickType_t xTicksToWait);

/* Lock the given UART receiver, so that no other task can read
   from it  Returns pdPASS if the lock is acquired. */
BaseType_t UART_16550_rx_lock(int UART,
			      TickType_t xTicksToWait);

/* Unlock the given UART receiver so that other tasks can read from
   it. */ 
void UART_16550_rx_unlock(int UART);

/* Try to read a character from the UART */
BaseType_t UART_16550_get_char(int UART, char *ch,
			       TickType_t xTicksToWait);

/* Try to read a string from the UART */
BaseType_t UART_16550_read_string(int UART,
				  char *s,
				  int maxLength,
				  TickType_t xTicksToWait);

// Return the number of characters available
int UART_16550_chars_available(int UART_number);

// Flush the UART receiver
void UART_16550_flush_rx(int UART_number);

#endif
