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
 * This file provides the driver table for the FreeRTOS 16550 device
 */

#include<uart_driver_table.h>
#include<UART_16550.h>

UART_driver_table_t ANSI_uart = {
  UART_16550_init,
  UART_16550_configure,
  UART_16550_tx_lock,
  UART_16550_tx_unlock,
  UART_16550_put_char,
  UART_16550_write_string,
  UART_16550_rx_lock,
  UART_16550_rx_unlock,
  UART_16550_get_char,
  UART_16550_read_string,
  UART_16550_flush_rx,
  UART_16550_chars_available,
};
