/* Newlib does not implement some functions because they are
   platform-specific.  Sometimes it will cause an error such as :

   ../ld: ../../libc.a(libc_a-closer.o): in function `_close_r':
   closer.c:(.text._close_r+0x20): warning: _close is not implemented
   and will always fail

   This file allows us to provide functions that will fix the linker
   error, but will actually do nothing.

*/

/* NOTE: All of these names are linked to a single function that just
   goes into an infinte loop
*/
  
  .text
  .func   _close_r
  .global _close_r
  .endfunc
  
  .func   _fstat_r
  .global _fstat_r
  .endfunc


  .func   _getpid_r
  .global _getpid_r
  .endfunc

  .func   _isatty_r
  .global _isatty_r
  .endfunc

  .func   _link_r
  .global _link_r
  .endfunc

  .func   _open_r
  .global _open_r
  .endfunc

  .func   _unlink_r
  .global _unlink_r
  .endfunc

  .func   _kill_r
  .global _kill_r
  .endfunc

  .func   _lseek_r
  .global _lseek_r
  .endfunc

  .func   _read_r
  .global _read_r
  .endfunc

  .func   _write_r
  .global _write_r
  .endfunc
  
_close_r: 
_fstat_r: 
_getpid_r:  
_isatty_r:  
_link_r:  
_open_r:  
_unlink_r:  
_kill_r:  
_lseek_r: 
_read_r:  
_write_r:

hangup: 
  j   hangup
  
