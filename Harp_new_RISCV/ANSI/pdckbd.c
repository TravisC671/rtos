/* PDCurses */

#include <curses.h>
#include <curspriv.h>
#include <ANSI_terminal.h>
#include <uart_driver_table.h>

#include "../common/acs437.h"

/************************************************************************
 *    Table for key code translation of special keys. See curses.h      *
 *    for complete list of special keys                                 *
 ************************************************************************/

static uint32_t key_table[] =
{
  // 0x80
  -1,             KEY_F(1),       KEY_F(2),       KEY_F(3),
  KEY_F(4),       KEY_F(5),       KEY_F(6),       KEY_F(7),
  // 0x88
  KEY_F(9),       KEY_F(10),      KEY_F(11),      KEY_F(12),
  -1,             -1,             -1,             -1,  
  // 0x90
  KEY_UP,         KEY_DOWN,       KEY_RIGHT,      KEY_LEFT,
  KEY_IC,         KEY_HOME,       KEY_PPAGE,      KEY_DC,
  // 0x98
  KEY_END,        KEY_NPAGE,      -1,             -1,
  -1,             -1,             -1,             -1,  
  // 0xA0
  -1,             -1,             -1,             -1,  
  -1,             -1,             -1,             -1,  
  -1,             -1,             -1,             -1,  
  -1,             -1,             -1,             -1,  
  // 0xB0
  -1,             -1,             -1,             -1,  
  -1,             -1,             -1,             -1,  
  -1,             -1,             -1,             -1,  
  -1,             -1,             -1,             -1,  
  // 0xC0
  -1,             -1,             -1,             -1,  
  -1,             -1,             -1,             -1,  
  -1,             -1,             -1,             -1,  
  -1,             -1,             -1,             -1,  
  // 0xD0
  -1,             -1,             -1,             -1,  
  -1,             -1,             -1,             -1,  
  -1,             -1,             -1,             -1,  
  -1,             -1,             -1,             -1,  
  // 0xE0
  -1,             -1,             -1,             -1,  
  -1,             -1,             -1,             -1,  
  -1,             -1,             -1,             -1,  
  -1,             -1,             -1,             -1,  
  // 0xF0
  -1,             -1,             -1,             -1,  
  -1,             -1,             -1,             -1,  
  -1,             -1,             -1,             -1,  
  -1,             -1,             -1,             -1
};


/* Keyboard/mouse event check, called from wgetch(). Returns TRUE if
   there's an event ready to process. This function must be
   non-blocking. */
bool PDC_check_key(void)
{
  return ANSI_uart.chars_available(CURSES_UART);
}

/*  This is the core of flushinp(). It discards any pending key or
    mouse events, removing them from any internal queue and from the
    OS queue, if applicable. */
void PDC_flushinp(void)
{
  ANSI_uart.flush_rx(CURSES_UART);
}

/* Get the next available key, or mouse event (indicated by a return
   of KEY_MOUSE), and remove it from the OS' input queue, if
   applicable. This function is called from wgetch(). This function
   may be blocking, and traditionally is; but it need not be. If a
   valid key or mouse event cannot be returned, for any reason, this
   function returns -1. Valid keys are those that fall within the
   appropriate character set, or are in the list of special keys found
   in curses.h (KEY_MIN through KEY_MAX). When returning a special key
   code, this routine must also set SP->key_code to TRUE; otherwise it
   must set it to FALSE. If SP->return_key_modifiers is TRUE, this
   function may return modifier keys (shift, control, alt), pressed
   alone, as special key codes; if SP->return_key_modifiers is FALSE,
   it must not. If modifier keys are returned, it should only happen
   if no other keys were pressed in the meantime; i.e., the return
   should happen on key up. But if this is not possible, it may return
   the modifier keys on key down (if and only if
   SP->return_key_modifiers is TRUE). */
int PDC_get_key(void)
{
  int32_t c;
  c = ANSI_getChar(CURSES_UART);
  // If it is a special key, then we need to translate it.
  if(c > 127)
    {
      SP->key_code = TRUE;
      SP->key_code = FALSE;
      c = key_table[c-128];
    }
  else
    {
      SP->key_code = FALSE;
      // uart_put_char(CURSES_UART,c);
    }
  return c;
}

/* Called from has_mouse(). Reports whether mouse support is
   available. Can be a static TRUE or FALSE, or dependent on
   conditions. Note: Activating mouse support should depend only on
   PDC_mouse_set(); don't expect the user to call has_mouse()
   first. */
bool PDC_has_mouse(void)
{
  return FALSE;
}

/* Called from PDC_return_key_modifiers(). If your platform needs to
   do anything in response to a change in SP->return_key_modifiers, do
   it here. Returns OK or ERR, which is passed on by the caller. */
int PDC_modifiers_set(void)
{
    return OK;
}

/* Called by mouse_set(), mouse_on(), and mouse_off() -- all the
   functions that modify SP->_trap_mbe. If your platform needs to do
   anything in response to a change in SP->_trap_mbe (for example,
   turning the mouse cursor on or off), do it here. Returns OK or ERR,
   which is passed on by the caller. */
int PDC_mouse_set(void)
{
  return OK;
}

/*  Set keyboard input to "binary" mode. If you need to do something
    to keep the OS from processing ^C, etc. on your platform, do it
    here. TRUE turns the mode on; FALSE reverts it. This function is
    called from raw() and noraw(). */
void PDC_set_keyboard_binary(bool on)
{
}

