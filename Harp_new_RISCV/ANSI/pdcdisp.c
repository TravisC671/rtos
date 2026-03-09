/* PDCurses */

#ifdef USE_FREERTOS
#include <FreeRTOS.h>
#else
#include<stdint.h>
typedef uint32_t TickType_t;
typedef long BaseType_t;
#endif

#include <curspriv.h>
#include <curses.h>
#include <ANSI_terminal.h>
#include <stdint.h>
#include <uart_driver_table.h>

// # define PDC_LOG(x) if (SP && SP->dbfp) PDC_debug x

static uint8_t curstoreal[16] =
{
    FG_BLACK, FG_BLUE, FG_GREEN, FG_CYAN, FG_RED,
    FG_MAGENTA, FG_YELLOW, FG_WHITE
};

/* Called at the end of doupdate(), this function finalizes the update
   of the physical screen to match the virtual screen, if necessary,
   i.e. if updates were deferred in PDC_transform_line(). */
void PDC_doupdate(void)
{
}

/* position hardware cursor at (y, x) */
void PDC_gotoyx(int row, int col)
{
  PDC_LOG(("PDC_gotoxy - called. row: %d Col: %d\n",row,col));
  ANSI_moveTo(CURSES_UART,row,col);
}

void  _set_attributes(attr_t oldattr,attr_t attr)
{
  unsigned color,oldcolor;
  attr_t changes,remaining,oldremaining;
  short fore,back;
  int tmp;
  char str[256];
  static int line = 1;
  static int numrev = 0;

  ANSI_clearAttrs(CURSES_UART);
  
  PDC_LOG(("setting attributes %08X\n",attr));
  line++;
  
  /* // separate the color from the remaining attributes */
  color = PAIR_NUMBER(attr);
  oldcolor = PAIR_NUMBER(oldattr);
  //  if(color != oldcolor)
  {
    pair_content(color,&fore,&back);
    fore =curstoreal[fore];
    back =curstoreal[back] + 10;
    PDC_LOG((" Setting color %08X fore=%d back=%d\n",color,fore,back));
    ANSI_setColor(CURSES_UART,fore);
    ANSI_setColor(CURSES_UART,back);
    PDC_LOG(("set color: FG=%d  BG=%d\n",fore,back));
  }
  
  // handle non-color attributes
  remaining = attr & 0x00FF0000; 
  oldremaining = oldattr & 0x00FF0000;
  // set a bit for every attribute that is being changed
  changes = remaining ^ oldremaining;
 
  if(changes & A_ALTCHARSET)
    {
      if(remaining & A_ALTCHARSET)
	{
	  PDC_LOG(("Selecting Alternate Character Set\n"));
	  ANSI_altCharSet(CURSES_UART);
	}
      else
	{
	  PDC_LOG(("Selecting Normal Character Set\n"));
	  ANSI_normalCharSet(CURSES_UART);
	}	
    }

  // Handle turning off any attributes
  if(remaining & A_ITALIC)
    {
      PDC_LOG(("Toggling Italics\n"));
      ANSI_toggleItalics(CURSES_UART);
    }
  if(remaining & A_UNDERLINE)
    {
      PDC_LOG(("Toggling Underline\n"));
      ANSI_toggleUnderline(CURSES_UART);
    }
  if(remaining & A_REVERSE)
    {
      PDC_LOG(("Toggling Reverse Video\n"));
      ANSI_toggleReverse(CURSES_UART);
    }
  if(remaining & A_BLINK)
    {
      PDC_LOG(("Toggling Blink\n"));
      ANSI_toggleBlinking(CURSES_UART);
    }
  if(remaining & A_BOLD)
    {
      PDC_LOG(("Toggling Bold\n"));
      ANSI_toggleBold(CURSES_UART);
    }
  if(remaining & A_LEFT)
    {
      PDC_LOG(("Toggling Leftline\n"));
      ANSI_toggleBold(CURSES_UART);
    }
  if(remaining & A_RIGHT)
    {
      PDC_LOG(("Toggling Rightline\n"));
      ANSI_toggleBold(CURSES_UART);
    }

  
}

/* The core output routine. It takes len chtype entities from srcp (a
   pointer into curscr) and renders them to the physical screen at
   line lineno, column x. It must also translate characters 0-127 via
   acs_map[], if they're flagged with A_ALTCHARSET in the attribute
   portion of the chtype. Actual screen updates may be deferred until
   PDC_doupdate() if desired (currently done with SDL and X11). */
void PDC_transform_line(int lineno, int x, int len, const chtype *srcp)
{
  // assume attributes don't change between calls to PDC_transform_line
  static attr_t old_attr = 0; 
  attr_t attr;
  char c;
  int i;

  PDC_LOG(("PDC_transform_line() - called: lineno=%d\n", lineno));

  // get the current attribute settings
  // old_attr = *srcp & (A_ATTRIBUTES ^ A_ALTCHARSET);

  // move the cursor (need to track cursor position and avoid this if
  // possible)
  ANSI_moveTo(CURSES_UART,lineno,x);

  ANSI_uart.tx_lock(CURSES_UART,portMAX_DELAY);
  for (i = 0; i< len; i++)
    {
      // get attributes for current character
      attr = srcp[i] & A_ATTRIBUTES;

      PDC_LOG(("  Old attr: %08X  New attr: %08X  Char:%c\n", old_attr, attr,srcp[i]&0x7F));
      
      if (attr != old_attr)
        {
	  _set_attributes(old_attr,attr);
	  old_attr = attr;
	}
      c = srcp[i] & 0x7F;
      ANSI_uart.put_char(CURSES_UART,c,portMAX_DELAY);
    }
  ANSI_uart.tx_unlock(CURSES_UART);
}


