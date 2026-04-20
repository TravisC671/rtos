
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
}


/* Linker script to place sections and symbol values. Should be used together
 * with other linker script that defines memory regions ROM and RAM.
 * It references following symbols, which must be defined in code:
 *   _Reset_Handler : Entry of reset handler
 *
 * It defines following symbols, which code can use without definition:
 *   __copy_table_start__
 *   __copy_table_end__
 *   __zero_table_start__
 *   __zero_table_end__
 *   __etext
 *   __data_start__
 *   __data_end__
 *   __bss_start__
 *   __bss_end__
 *   __StackTop
 *   __HeapBase
 *   __HeapTop
 */
ENTRY(_Reset_Handler)

SECTIONS
{
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

    . = ALIGN(4);
    __copy_table_end__ = .;
  } > ROM

  .zero.table (READONLY):
  {
    . = ALIGN(4);
    __zero_table_start__ = .;
    LONG (__bss_start__)
    LONG ((__bss_end__ - __bss_start__) / 4)
    . = ALIGN(4);
    __zero_table_end__ = .;
  } > ROM

  . = ALIGN(4);

  __etext = .;
  __End_Text__ = .;

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

  /* __ebss is the end of last bss section */
  __ebss = .;
  __End_Bss__ = .;

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

    . = ALIGN(8);
    __end__ = .;
    PROVIDE(_end = .);
    PROVIDE(__end = __ebss);

  .heap :
  {
    . = ALIGN(8);
     __HeapBase = .;
  } > HEAP

}
