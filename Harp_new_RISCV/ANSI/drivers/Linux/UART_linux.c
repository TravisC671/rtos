/* 
 * File:   UART.c
 * Author: Larry Pyeatt
 * 
 * Description: Provides functions that are equivalent to the UART
 * functions used on the embedded platform.  This allows running
 * PDCurses on a Linux system using stdin/stdout as the terminal
 * interface.
 */

#define UART0 0


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <stdint.h>
#include <fcntl.h>
#include <poll.h>

#include <ANSI_device.h>
#include <uart_driver_table.h>
#include <UART_linux.h>

static int initialized = 0;

// For Unix/Linux, we are going to put the terminal into raw mode, so
// we need to store the original terminal settings.
struct termios orig_termios;

// When our program exits, we want to restore the terminal settings.
void disableRawMode() {
  showCursor(UART0);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}
// This function saves the current terminal settings then puts it in
// raw mode.
void enableRawMode() {
  struct termios raw;
  // make sure that the terminal settings will be restored whenever
  // the exit() funcion is called.
  atexit(disableRawMode);
  // now we can set the terminal to raw mode.
  tcgetattr(STDIN_FILENO, &orig_termios);
  raw = orig_termios;
  raw.c_lflag &= ~(ICANON | ECHO);
  raw.c_oflag &= ~(OPOST);
  tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

// Unix/Linux equivalent of uart_init.  
void UART_linux_init()
{
  enableRawMode();
  initialized = 1;
}

// Unix/Linux equivalent of uart_configure.  We are actually just
// using a system terminal, so we ignore all of the settings.
void UART_linux_configure(int UART,int baud,int parity,int bits,int stop_bits)
{
  if(!initialized)
    {
      fprintf(stderr,
	      "Function UART_linux_configure was called before uart_init was"
	      " called.\n"
	      "You must initialize the UART driver before tying to use it.\n"
	      );
      exit(2);
    }
  if(UART > 0)
    {
      fprintf(stderr,"Attempted to initialize UART%d\n",UART);
      fprintf(stderr,"The driver only supports UART0\n");
      exit(1);
    }
  enableRawMode();
  fflush(stdout);
  initialized = 2;
  setColor(UART0,BG_BLACK);
  setColor(UART0,FG_WHITE);
} 

void checkinit(char *func,int UART)
{
  if(!initialized == 2)
    {
      fprintf(stderr,
	      "Function %s was called before uart_configure was called for \n"
	      "UART%d. You must configure the UART before tying to use it.\n",
	      func,UART);
      exit(2);
    }
}

BaseType_t UART_linux_tx_lock(int UART,
                              TickType_t xTicksToWait)
{
  checkinit("UART_linux_tx_lock",UART);
}

void UART_linux_tx_unlock(int UART)
{
  checkinit("UART_linux_tx_unlock",UART);
}

// Unix/Linux equivalent of putU1
BaseType_t UART_linux_put_char(int UART,
			       char c,
			       TickType_t xTicksToWait)
{
  checkinit("UART_linux_put_char",UART);
  putchar(c);
  fflush(stdout);
} 

// Output NULL-terminated string to UART1
BaseType_t UART_linux_write_string(int UART,
                                   char *s,
                                   TickType_t xTicksToWait)
{
  checkinit("UART_linux_write_string",UART);
  while (*s)	  // loop until *s = \0, end of string
    UART_linux_put_char(UART,*s++,xTicksToWait);
} 


BaseType_t UART_linux_rx_lock(int UART,
                              TickType_t xTicksToWait)
{
  checkinit("UART_linux_rx_lock",UART);
}

void UART_linux_rx_unlock(int UART)
{
  checkinit("UART_linux_rx_unlock",UART);
}



// Unix/Linux equivalent of getU1 
BaseType_t UART_linux_get_char(int UART, char *ch,
                               TickType_t xTicksToWait)
{
  int flags,success=pdPASS;
  checkinit("UART_linux_get_char",UART);
  if(xTicksToWait != portMAX_DELAY)
    {
      usleep(xTicksToWait);  
      flags = fcntl(STDIN_FILENO, F_GETFL,0);  // get current file settings
      fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);// disable blocking
      success = read(STDIN_FILENO, ch, 1);    // try to read
      fcntl(STDIN_FILENO, F_SETFL, flags);     // restore file settings
      if(success < 0)                          
	*ch =  0xFF;
    }
  else
    success = read(STDIN_FILENO, ch, 1); 
  return success;
} 

    /* Try to read a string from the UART */
BaseType_t UART_linux_read_string(int UART,
                                  char *s,
                                  int maxLength,
                                  TickType_t xTicksToWait)
{
  char c;
  int length=0;
  BaseType_t result;
  // Read characters until maxLength or until we get an ASCII newline
  // or return character, or until we timeout.
  do
   {
     result = UART_linux_get_char(UART,&c,xTicksToWait);
     if(result == pdPASS)
       *(s++) = (c == '\n') || (c == '\r') ? 0 : c;
     length++;
   }
  while (c != '\n' && c != '\r' && length < maxLength && result == pdPASS);
  // make sure it is null terminated
  if(length == maxLength)
      s[length-1]=0;
  return result;
}

void UART_linux_flush_rx(int UART)
{
  char ch;
  checkinit("UART_linux_flush_rx",UART);
  while(UART_linux_chars_available(UART))
    read(STDIN_FILENO, &ch, 1);
}

  
int UART_linux_chars_available(int UART)
{
  checkinit("UART_linux_chars_available",UART);
  struct pollfd pf = {STDIN_FILENO, POLLIN, 0};  
  return poll(&pf,1,0);
}

  
