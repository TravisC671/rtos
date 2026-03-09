/* 
 * File:   driver_table.c
 * Author: Larry Pyeatt
 * 
 * Description:
 *
 * The ANSI terminal code provides an interface to different UART
 * device drivers.  The assumption is that the UART device is
 * connected to an ANSI compatible terminal.  This file provides a
 * data type that maps the driver-agnostic function names to specific
 * driver functions.  At compile-time, we will use a driver-specific C
 * file to provide the actual mapping.
 * 
 * This was done because our system has different types of UART, and
 * each type has a driver that provides these functions.  This makes
 * it a little easier to change the ncurses UI from one UART to
 * another by selecting which driver table is compiled and linked.
 *
 * This file provides the driver table for the Linux stdin/stdout "UART"
 */

#include<uart_driver_table.h>
#include<UART_linux.h>

UART_driver_table_t ANSI_uart = {
  UART_linux_init,
  UART_linux_configure,
  UART_linux_tx_lock,
  UART_linux_tx_unlock,
  UART_linux_put_char,
  UART_linux_write_string,
  UART_linux_rx_lock,
  UART_linux_rx_unlock,
  UART_linux_get_char,
  UART_linux_read_string,
  UART_linux_flush_rx,
  UART_linux_chars_available,
};
