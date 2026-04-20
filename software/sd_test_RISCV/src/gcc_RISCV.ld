
__ROM_BASE = 0x00000000;
__ROM_SIZE = 0x40000;

__RAM_BASE = 0x08000000;
__RAM_SIZE = 0x8000000;

/* System/IRQ stack at the very start of DDR2 */
__STACK_SIZE = 0x00001000;

MEMORY
{
  ROM   (rx)   : ORIGIN = __ROM_BASE,  LENGTH = __ROM_SIZE
  RAM   (rwx)  : ORIGIN = __RAM_BASE,  LENGTH = __RAM_SIZE
}

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

  /* ---- DDR2 layout: stack, .data, .bss, heap ---- */

  /* System/IRQ stack at the start of DDR2.
     SP starts at __StackTop and grows down toward __StackLimit. */
  __StackLimit = ORIGIN(RAM);
  __StackTop = __StackLimit + __STACK_SIZE;
  PROVIDE(__freertos_irq_stack_top = __StackTop);

  /* Initialized data (copied from ROM by startup code) */
  .data (__StackTop) : AT (__etext)
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

  __edata = .;

  /* Zero-initialized data */
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

  __ebss = .;
  __End_Bss__ = .;

  /* Heap starts at the next 4K boundary after .bss and extends to end of DDR2.
     Newlib sbrk uses _end as the heap base. */
  . = ALIGN(4096);
  __end__ = .;
  PROVIDE(end = .);
  PROVIDE(_end = .);
  PROVIDE(__end = .);
  __HeapBase = .;
  __HeapStart = .;
  __HeapLimit = ORIGIN(RAM) + LENGTH(RAM);
  __HeapTop = __HeapLimit;

  ASSERT(__HeapBase < __HeapLimit, "No space left for heap")
}
