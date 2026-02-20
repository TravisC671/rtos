
__ROM_BASE = 0x00000000;
__ROM_SIZE = 0x00020000;

__RAM_BASE = 0x20000000;
__RAM_SIZE = 0x00020000;

/* we will put the stack at the end of RAM so it can grow downwards */
__STACK_SIZE = 0x00000400;

/* FreeRTOS allocates the heap in .bss or .data, so no space is
   required here but we may want to check memory usage. */
__HEAP_SIZE = 0x00010000;
   




MEMORY
{
  ROM   (rx)  : ORIGIN = __ROM_BASE, LENGTH = __ROM_SIZE
  RAM   (rwx) : ORIGIN = __RAM_BASE, LENGTH = __RAM_SIZE
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
ENTRY(Reset_Handler)

SECTIONS
{
  .text :
  {
    __Begin_Text__ = .;
    KEEP(*(.vectors))
    *(.text*)

    KEEP(*(.init))
    KEEP(*(.fini))

    *(.rodata*)
    . = ALIGN(4);
  } > ROM

  /* __exidx_start and __exidx_end need to be defined */
  __exidx_start = .;
  .ARM.exidx :
  {
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    . = ALIGN(4);
  } > ROM
  __exidx_end = .;


  .copy.table :
  {
    . = ALIGN(4);
    __copy_table_start__ = .;

    LONG (__etext)
    LONG (__data_start__)
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

  .zero.table :
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

  __etext = .;
  __End_Text__ = .;

  /**
   * Location counter can end up 2byte aligned with narrow Thumb code but
   * __etext is assumed by startup code to be the LMA of a section in RAM
   * which must be 4byte aligned 
   */
  .data : AT (__etext) 
  {
    __Begin_Data__ = .;
    __data_start__ = .;
    *(.data)
    *(.data.*)
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
    PROVIDE(__StackTop = .);
  } > RAM
  PROVIDE(__stack = __StackTop);

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
  
  /* We can also use the heap_useNewlib.c heap code, which replaces
     the default Newlib sbrk.c with a better implementation.  It needs
     __Heapbase and __HeapTop to be defined, and can make sure that
     the heap does not overflow the system stack.  You can turn on
     stack checking in FreeRTOS to (hopefully) also detect the system
     stack overflowing the heap. */
  __HeapBase = __ebss;
  .heap (__HeapBase) (COPY) :
  {
    . = ALIGN(8);
    __end__ = .;
    PROVIDE(end = .); 
    PROVIDE(_end = .);
    PROVIDE(__end = __ebss);
    . = __StackLimit - 1;
    __HeapTop =  .;
  } > RAM

 ASSERT(__StackLimit > __HeapTop, "Data/BSS overflowed with stack")


}
