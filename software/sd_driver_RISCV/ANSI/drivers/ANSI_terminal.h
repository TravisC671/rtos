/* 
 A very simple set of functions to perform operations on ANSI terminals.
 Author: Larry D. Pyeatt
 Date:  9-1-2009
 (C) All rights reserved


 These functions rely on the functions provided by the lower level
 device-specific drivers.
 
*/


#ifndef ANSI_DEVICE_H
#define ANSI_DEVICE_H

#include <stdint.h>

#ifdef USE_FREERTOS
#include <FreeRTOS.h>
#else
typedef uint32_t TickType_t;
typedef long BaseType_t;
#define pdPASS 1
#define pdFAIL 0
#define portMAX_DELAY (-1)
#endif

#ifndef CURSES_UART
#define CURSES_UART 0
#endif

// Read a char from keyboard, and handle the fancy keys that generate
// multi-byte escape sequences
uint8_t ANSI_getChar(int uart);

// reset all modes (styles and colors)
void ANSI_resetScreen(int uart);

// Get the current cursor position
void ANSI_getCursorPos(int uart,int *x,int *y);

// Get the number of columns and rows on the screen
void ANSI_getScreenSize(int uart,int *width, int *height);

// Tell terminal to clear the screen
void ANSI_clear(int uart);

// Tell terminal to clear from cursor to end of line
void ANSI_cleartoeol(int uart);

// Tell terminal to save the current cursor position
void ANSI_saveCursor(int uart);

// Tell terminal to restore the previouly saved cursor position
void ANSI_restoreCursor(int uart);

// Tell terminal to hide the cursor
void ANSI_hideCursor(int uart);

// Tell terminal to show the cursor
void ANSI_showCursor(int uart);

// Tell terminal to move the cursor
void ANSI_moveTo(int uart,int row, int col);

// Tell terminal to move the cursor left one character
void ANSI_moveLeft(int uart);

// Tell terminal to move the cursor right one character
void ANSI_moveRight(int uart);

// Draw a rectangle on the screen
int ANSI_drawBorder(int uart,int row, int col, int width);

// Select alternate character set
void ANSI_altCharSet(int uart);

// Select normal character set
void ANSI_normalCharSet(int uart);

// reset all character attributes to default
void ANSI_clearAttrs(int uart);

// start/stop blinking
void ANSI_toggleBlinking(int uart);

// start/stop italics
void ANSI_toggleItalics(int uart);

// start/stop bold
void ANSI_toggleBold(int uart);

// start/stop underline
void ANSI_toggleUnderline(int uart);

// start/stop strikethrough
void ANSI_toggleStrikethrough(int uart);

// start/stop reverse video
void ANSI_toggleReverse(int uart);

// start/stop reverse video
void ANSI_toggleDim(int uart);

void ANSI_bell(int uart);


// Change foreground or background color
void ANSI_setColor(int uart,int color);

// Use these names when you are using getChar and checking for
// specific keys being pressed
#define    Key_F1	0x81
#define    Key_F2	0x82
#define    Key_F3	0x83
#define    Key_F4	0x84
#define    Key_F5	0x85
#define    Key_F6	0x86
#define    Key_F7	0x87
#define    Key_F8	0x88
#define    Key_F9	0x89
#define    Key_F10	0x8A
#define    Key_F11	0x8B
#define    Key_F12	0x8C
#define    Key_UpArrow	0x90
#define    Key_DnArrow	0x91
#define    Key_RgtArrow	0x92
#define    Key_LftArrow	0x93
#define    Key_Insert	0x94
#define    Key_Home	0x95
#define    Key_PgUp	0x96
#define    Key_Del	0x97
#define    Key_End	0x98
#define    Key_PgDn	0x99
#define    Key_ESC	0x1B


// These colors are used with the setColor function
#define FG_BLACK   30
#define FG_RED     31
#define FG_GREEN   32
#define FG_YELLOW  33
#define FG_BLUE    34
#define FG_MAGENTA 35
#define FG_CYAN    36
#define FG_WHITE   37

#define BG_BLACK   40
#define BG_RED     41
#define BG_GREEN   42
#define BG_YELLOW  43
#define BG_BLUE    44
#define BG_MAGENTA 45
#define BG_CYAN    46
#define BG_WHITE   47

#endif
