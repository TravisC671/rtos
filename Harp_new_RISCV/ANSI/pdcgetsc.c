/* PDCurses */


#include <curspriv.h>
#include <ANSI_terminal.h>

/* return width of screen/viewport */

int PDC_get_columns(void)
{
  int rows,cols;
  ANSI_getScreenSize(CURSES_UART,&cols,&rows);
  return cols;
}

/* get the cursor size/shape */

int PDC_get_cursor_mode(void)
{
  PDC_LOG(("PDC_get_cursor_mode() - called\n"));
  return 0;
}

/* return number of screen rows */

int PDC_get_rows(void)
{
  int rows,cols;
  ANSI_getScreenSize(CURSES_UART,&cols,&rows);
  return rows;
}
