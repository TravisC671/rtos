/* 
 * File:   uart_driver_table.h
 * Author: Larry Pyeatt
 * 
 * Description:
 *
 * The ANSI terminal platform code provides an interface to different
 * UART device drivers.  The assumption is that the UART device is
 * connected to an ANSI compatible terminal.  This file provides a
 * data type that maps the driver-independent function names to specific
 * driver functions.  At compile-time, we will use a driver-specific C
 * file to provide the actual mapping.
 * 
 * This was done because our system has different types of UART, and
 * each type has a driver that provides these functions.  Using a
 * driver table makes it a little easier to change the ncurses UI from
 * one UART to another by selecting which driver table is compiled and
 * linked, while still allowing us to use other UART drivers for other
 * purposes.
 */

#ifndef UART_DRIVER_TABLE_H
#define UART_DRIVER_TABLE_H

#ifdef USE_FREERTOS
#include <FreeRTOS.h>
#else
#include<stdint.h>
typedef uint32_t TickType_t;
typedef long BaseType_t;
#endif


typedef struct {
  // init function for a driver
  void(*init)();          
  // the configure function for a driver
  void(*configure)(int UART,int baud,int parity,int bits,int stop_bits);
  // the tx_lock function for a driver
  BaseType_t(*tx_lock)(int UART,TickType_t xTicksToWait);
  // the tx_unlock function for a driver
  void(*tx_unlock)(int UART);
  // the put_char function for a driver
  BaseType_t(*put_char)(int UART,
			char c,
			TickType_t xTicksToWait);
  // the write_string function for a driver
  BaseType_t(*write_string)(int UART,
			    char *s,
			    TickType_t xTicksToWait);
  // the rx_lock function for a driver
  BaseType_t(*rx_lock)(int UART,
		       TickType_t xTicksToWait);
  // the rx_unlock function for a driver
  void(*rx_unlock)(int UART);
  // the get_char function for a driver
  BaseType_t(*get_char)(int UART, char *ch,
			TickType_t xTicksToWait);
  // the read_string function for a driver
  BaseType_t(*read_string)(int UART,
			   char *s,
			   int maxLength,
			   TickType_t xTicksToWait);
  // the flush_rx function for a driver
  void(*flush_rx)(int UART);
  // the chars_available function for a driver
  int(*chars_available)(int UART);
}UART_driver_table_t;

// We will use a specific uart driver table defined by whatever driver
// we compile and link to our code.
extern UART_driver_table_t ANSI_uart;
  
#endif
