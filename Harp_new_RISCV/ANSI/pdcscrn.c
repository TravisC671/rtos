/* PDCurses */


#include <curses.h>

#ifdef FreeRTOS
#include <FreeRTOS.h>
#include <task.h>
#else
#include <unistd.h>
#define vTaskDelay(x) usleep(x)
#define pdMS_TO_TICKS(x) (ms * 1000)
#endif

#include <curspriv.h>
#include <ANSI_terminal.h>
#include <uart_driver_table.h>



/* Returns TRUE if init_color() and color_content() give meaningful
   results, FALSE otherwise. Called from can_change_color(). */
bool PDC_can_change_color(void)
{
  PDC_LOG(("PDC_can_change_color() - called.\n"));
  return FALSE;
}

/* The core of color_content(). This does all the work of that
   function, except checking for values out of range and null
   pointers. */
int PDC_color_content(short color, short *red, short *green, short *blue)
{
  // Need to add a table to lookup the colors.
  PDC_LOG(("PDC_color_content() - called.\n"));
  // switch(color)
    
  *red = 1000;
  *green = 1000;
  *blue = 1000;
  return OK;
}

/* The core of init_color(). This does all the work of that function,
   except checking for values out of range. */
int PDC_init_color(short color, short red, short green, short blue)
{
  PDC_LOG(("initializing color %d %d %d %d\n",color,red,green,blue));
  return OK;
}

/* The non-portable functionality of reset_prog_mode() is handled here
   -- whatever's not done in _restore_mode(). In current ports: In
   OS/2, this sets the keyboard to binary mode; in Windows console, it
   enables or disables the mouse pointer to match the saved mode; in
   others it does nothing. */
void PDC_reset_prog_mode(void)
{
  PDC_LOG(("PDC_reset_prog_mode() - called.\n"));
}

/* The same thing, for reset_shell_mode(). In OS/2 and Windows
   console, it restores the default console mode; in others it does
   nothing. */
void PDC_reset_shell_mode(void)
{
  PDC_LOG(("PDC_reset_shell_mode() - called.\n"));
}


/* This does the main work of resize_term(). It may respond to
   non-zero parameters, by setting the screen to the specified size;
   to zero parameters, by setting the screen to a size chosen by the
   user at runtime, in an unspecified way (e.g., by dragging the edges
   of the window); or both. It may also do nothing, if there's no
   appropriate action for the platform. */
int PDC_resize_screen(int nlines, int ncols)
{
    PDC_LOG(("PDC_resize_screen() - called. Lines: %d Cols: %d\n",
             nlines, ncols));
    return OK;
}

/* Called from _restore_mode() in kernel.c, this function does the
   actual mode changing, if applicable. Currently used only in DOS and
   OS/2. */
void PDC_restore_screen_mode(int i)
{
}

/* Called from _save_mode() in kernel.c, this function saves the
   actual screen mode, if applicable. Currently used only in DOS and
   OS/2. */
void PDC_save_screen_mode(int i)
{
}

/* The platform-specific part of endwin(). It may restore the image of
   the original screen saved by PDC_scr_open(), if the
   PDC_RESTORE_SCREEN environment variable is set; either way, if
   using an existing terminal, this function should restore it to the
   mode it had at startup, and move the cursor to the lower left
   corner. (The X11 port does nothing.) */

void PDC_scr_close(void)
{
    PDC_LOG(("PDC_scr_close() - called\n"));
}

/* Free any memory allocated by PDC_scr_open(). Called by delscreen(). */
void PDC_scr_free(void)
{
PDC_LOG(("PDC_scr_free() - called\n"));
#ifdef PDCDEBUG
  traceoff();
#endif
}

/* The platform-specific part of initscr(). It must initialize
   acs_map[] (unless it's preset) and several members of SP, including
   mouse_wait, orig_attr (and if orig_attr is TRUE, orig_fore and
   orig_back), mono, _restore and _preserve. If using an existing
   terminal, and the environment variable PDC_RESTORE_SCREEN is set,
   this function may also store the existing screen image for later
   restoration by PDC_scr_close(). */
int PDC_scr_open(void)
{
  int i,j;

  // create mapping from curses colors to ANSI colors
  /* for (i = 0; i < 16; i++) */
  /*   pdc_curstoreal[realtocurs[i]] = i; */

  PDC_LOG(("PDC_scr_open() - called\n"));

  COLORS=8;
  // COLOR_PAIRS=256; is hard-coded in curspriv.h 
  /* for(i=0;i<COLORS;i++) */
  /*   for(j=0;j<COLORS;j++) */
  /*     { */
  /* 	int index = i*COLORS+j; */
  /* 	SP->atrtab[index].f = i+30; */
  /* 	SP->atrtab[index].b = j+40; */
  /* 	SP->atrtab[index].set = TRUE; */
  /* 	SP->atrtab[index].count = index; */
  /*     } */

  // direct mapping for alternate character set
  //for(i=0;i<128;i++)
  //acs_map[i] = i;
  
    SP->audible = TRUE;
    SP->mono = FALSE;
    SP->orig_attr = FALSE;  // why?
    SP->termattrs = A_ALTCHARSET | A_ITALIC | A_UNDERLINE |
      A_REVERSE | A_BLINK | A_LEFT | A_RIGHT | A_BOLD;
    SP->alive = TRUE;
    //    SP->orig_attr = TRUE;
    //SP->orig_fore = 7;
    //SP->orig_back = 0;

    // reset all modes (styles and colors)

    ANSI_uart.init();
    ANSI_resetScreen(CURSES_UART);

    return OK;
}



