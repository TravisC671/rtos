/* 
 * File:   UART.c
 * Author: Larry Pyeatt
 * 
 * Description:
 * Provides functions that are equivalent to the UART functions used 
 * on the embedded platform.  This allows testing/debugging of the UI 
 * code on a Linux system, using stdin/stdout as the terminal interface.
 */

#ifndef UART_LINUX_H
#define UART_LINUX_H

// Unix/Linux equivalent of uart_init.  We are actually just using
// stdin/stdout, so there is nothing to initialize.
void UART_linux_init();

// Unix/Linux equivalent of uart_configure.  We are actually just
// using a system terminal, so we ignore all of the settings.
void UART_linux_configure(int UART,int baud,int parity,int bits,int stop_bits);

BaseType_t UART_linux_tx_lock(int UART,
                              TickType_t xTicksToWait);

void UART_linux_tx_unlock(int UART);

// Unix/Linux equivalent of putU1
BaseType_t UART_linux_put_char(int UART,
			       char c,
			       TickType_t xTicksToWait);

// Output NULL-terminated string to UART1
BaseType_t UART_linux_write_string(int UART,
                                   char *s,
                                   TickType_t xTicksToWait);

BaseType_t UART_linux_rx_lock(int UART,
                              TickType_t xTicksToWait);

void UART_linux_rx_unlock(int UART);

// Get a character
BaseType_t UART_linux_get_char(int UART, char *ch,
                               TickType_t xTicksToWait);

// Get String from UART
BaseType_t UART_linux_read_string(int UART,
                                  char *s,
                                  int maxLength,
                                  TickType_t xTicksToWait);
    
void UART_linux_flush_rx(int UART);
  
int UART_linux_chars_available(int UART);

  
#endif
