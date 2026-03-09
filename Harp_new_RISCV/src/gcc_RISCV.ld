
__ROM_BASE = 0x00000000;
__ROM_SIZE = 0x40000;

__RAM_BASE = 0x08000000;
__RAM_SIZE = 0x8000000;

/* we will put the stack at the end of RAM so it can grow downwards */
__STACK_SIZE = 0x00001000;

/* FreeRTOS allocates the heap in .bss or .data, so no space is
   required here but we may want to check memory usage. */
__HeapStart = 0x08100000;
__HeapTop   = __RAM_BASE + __RAM_SIZE - __STACK_SIZE;
__HEAP_SIZE = __HeapTop - __HeapStart;

MEMORY
{
  ROM   (rx)   : ORIGIN = __ROM_BASE,  LENGTH = __ROM_SIZE
  RAM   (rwx)  : ORIGIN = __RAM_BASE,  LENGTH = __RAM_SIZE
  HEAP  (rwx)  : ORIGIN = __HeapStart, LENGTH = __HEAP_SIZE
  /*  THEME (r)    : ORIGIN = 0x80000000, LENGTH = 0x200000 */
  /*   DEBUG (r)   : ORIGIN = 0x80200000, LENGTH = 0x200000 */
}


/* Linker script to place sections and symbol values. Should be used together
 * with other linker script that defines memory regions ROM and RAM.
 * It references following symbols, which must be defined in code:
 *   Reset_Handler : Entry of reset handler
 *
 * It defines following symbols, which code can use without definition:
 *   __exidx_start
 *   __exidx_end
 *   __copy_table_start__
 *   __copy_table_end__
 *   __zero_table_start__
 *   __zero_table_end__
 *   __etext
 *   __data_start__
 *   __preinit_array_start
 *   __preinit_array_end
 *   __init_array_start
 *   __init_array_end
 *   __fini_array_start
 *   __fini_array_end
 *   __data_end__
 *   __bss_start__
 *   __bss_end__
 *   __end__
 *   end
 *   __HeapLimit
 *   __StackLimit
 *   __StackTop
 *   __stack
 */
ENTRY(_Reset_Handler) 

SECTIONS
{
  /* .vectors : */
  /* { */
  /*   KEEP(*(.vectors)) */
  /* } > ROM */

  /* .init : */
  /* { */
  /*   KEEP(*(.init)) */
  /* } > ROM */

  .text :
  {
    __Begin_Text__ = .;
    KEEP(*(.init))
    KEEP(*(.text*))
    KEEP(*(.fini))
    . = ALIGN(4);
    *(.rodata*) 
    . = ALIGN(4);
  } > ROM

  . = ALIGN(4);

  /* __exidx_start and __exidx_end need to be defined */
  __exidx_start = .;
  .ARM.exidx :
  {
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    . = ALIGN(4);
  } > ROM
  __exidx_end = .;

  . = ALIGN(4);
  .copy.table (READONLY):
  {
    . = ALIGN(4);
    __copy_table_start__ = .;

    LONG (__etext)
    . = ALIGN(4);

    LONG (__data_start__)
    . = ALIGN(4);

    LONG ((__data_end__ - __data_start__) / 4)

    /* Add each additional data section here */
    /*
    LONG (__etext2)
    LONG (__data2_start__)
    LONG ((__data2_end__ - __data2_start__) / 4)
    */
    . = ALIGN(4);
    __copy_table_end__ = .;
  } > ROM

  .zero.table (READONLY):
  {
    . = ALIGN(4);
    __zero_table_start__ = .;
    LONG (__bss_start__)
    LONG ((__bss_end__ - __bss_start__) / 4)
    /* Add each additional bss section here */
    /*
    LONG (__bss2_start__)
    LONG ((__bss2_end__ - __bss2_start__) / 4)
    */
    . = ALIGN(4);
    __zero_table_end__ = .;
  } > ROM

  . = ALIGN(4);

  __etext = .;
  __End_Text__ = .;

  /**
   * Location counter can end up 2byte aligned with narrow Thumb code but
   * __etext is assumed by startup code to be the LMA of a section in RAM
   * which must be 4byte aligned 
   */
  . = ALIGN(4);
  .data : AT (__etext) 
  {
    . = ALIGN(4);
    __Begin_Data__ = .;
    __data_start__ = .;
    *(.data)
    *(.data.*)
    *(.sdata)
    *(.sdata.*)
    . = ALIGN(4);
    __data_end__ = .;
    __End_Data__ = .;
  } > RAM 

  /* add any special data sections here and add to copy table above */

  /* __edata is the end of last data section */
  __edata = .;
  .bss :
  {
    . = ALIGN(4);
    __bss_start__ = .;
    *(.bss)
    *(.bss.*)
    *(.sbss)
    *(.sbss.*)
    *(COMMON)
    . = ALIGN(4);
    __bss_end__ = .;
  } > RAM

  /* add any special bss sections here and add to .zero table above */


  /* __ebss is the end of last bss section */
  __ebss = .;
  __End_Bss__ = .;
  /* .stack (ORIGIN(RAM) + LENGTH(RAM) - __STACK_SIZE) (COPY) : */
  /* { */
  /*   . = ALIGN(8); */
  /*   __StackLimit = .; */
  /*   . = . + __STACK_SIZE; */
  /*   . = ALIGN(8); */
  /*   PROVIDE(__StackTop = .); */
  /* } > RAM */
  /* PROVIDE(__stack = __StackTop); */

  /* Put the system stack at the end of memory */
  .stack (ORIGIN(RAM) + LENGTH(RAM) - __STACK_SIZE) (COPY) :
  {
    . = ALIGN(4);
    __StackLimit = .;
    . = . + __STACK_SIZE;
    . = ALIGN(8);
    __StackTop = .;    
  } > RAM
  PROVIDE(__freertos_irq_stack_top = __StackTop); 

/* .heap (COPY) : */
  /* { */
  /*   . = ALIGN(8); */
  /*   __end__ = .; */
  /*   PROVIDE(end = .); /\* used by default newlib sbrk *\/ */
  /*   PROVIDE(_end = .); */
  /*   . = . + __HEAP_SIZE; */
  /*   . = ALIGN(8); */
  /*   __HeapLimit = .; */
  /* } > RAM */

  /* The heap can use all of the space between the end of the .bss
     section(s) and the beginning of the stack */

  
  /* By default, Newlib uses "end" to find where the heap starts.  In
   *  our case, It is at the end of the last bss section. 
   *  Some implementations of malloc/free use _end or __end */
  /*
  end = __ebss;
  .heap (COPY) :
  {
    . = ALIGN(8);
    __end__ = .;
    PROVIDE(end = .); 
    PROVIDE(_end = .);
    PROVIDE(__end = __ebss);
    . = . + __HEAP_SIZE;
    . = ALIGN(8);
    __HeapTop = .;
  } > RAM
  */
  
    . = ALIGN(8);
    __end__ = .;
    PROVIDE(_end = .);
    PROVIDE(__end = __ebss);


/* We can also use the heap_useNewlib.c heap code, which replaces
     the default Newlib sbrk.c with a better implementation.  It needs
     __Heapbase and __HeapTop to be defined, and can make sure that
     the heap does not overflow the system stack.  You can turn on
     stack checking in FreeRTOS to (hopefully) also detect the system
     stack overflowing the heap. */
  /* __HeapBase = __ebss; */

  /* .heap (COPY) : */
  /* { */
  /*   /\* . = HeapTop; *\/ */
  /*   PROVIDE(end = .);  */
  /*   . = . + __HEAP_SIZE; */
  /* } > HEAP */

/*  .theme : 
  {
    KEEP(*(.theme))
    . = ALIGN(8);
  } > THEME
*/

/*
  .debug (ORIGIN(DEBUG)) : 
  {
    KEEP(*(.debug))
    KEEP(*(.debug_abbrev))
    KEEP(*(.debug_str))
    KEEP(*(.comment))
    KEEP(*(.ARM.attributes))
    KEEP(*(.debug_loc))
    . = ALIGN(8);
  } > DEBUG
*/



  /* .heap (ORIGIN(HEAP)) : */
  /* { */
  /*   /\*    . = __HeapStart; *\/ */
  /*   . = ALIGN(4); */
  /*   PROVIDE( _HeapStart = . );    */

  /*   . += __HEAP_SIZE; */
    
  /*   __Heap_End = .; */

  /*   PROVIDE( _HeapTop = . );    */

  /*   . = ALIGN(4); */

  /* /\* /\\*   PROVIDE(_end = .); *\\/ *\/ */
  /* /\* /\\*   PROVIDE(__end = .); *\\/ *\/ */
  /* /\* /\\*   PROVIDE(end = .); *\\/ *\/ */
  /* } > HEAP */


 /* 
 ASSERT(__StackLimit > __HeapTop, "Data/BSS overflowed with stack")
*/

}
