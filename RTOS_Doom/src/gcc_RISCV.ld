
__RAM_BASE = 0x10000000;
__RAM_SIZE = 0x01000000;

/* __RAM_BASE = __ROM_BASE + __ROM_SIZE; */
/* __RAM_SIZE = 0x00200000; */

__STACK_SIZE = 0x00001000;

/*
__TASKS_BASE = __RAM_BASE + __RAM_SIZE;
__TASKS_SIZE = 0x00100000;*/          /* 1 MB */

__HEAP_SIZE =  0x00E00000;
__HeapStart = 0x10080000;
__HeapTop   = 0x11000000 - __STACK_SIZE;

MEMORY
{
  RAM   (rwx)  : ORIGIN = __RAM_BASE,    LENGTH = __RAM_SIZE
  /* ROM   (rx)   : ORIGIN = __ROM_BASE,    LENGTH = __ROM_SIZE
  TASKS (rwx)  : ORIGIN = __TASKS_BASE,  LENGTH = __TASKS_SIZE */
  HEAP  (rwx)  : ORIGIN = __HeapStart,   LENGTH = __HEAP_SIZE
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
  .text :
  {
    /* Hardware reset entry must be at ROM base (PC = 0x00000000).
       It holds a single "j _Reset_Handler" instruction. */
    KEEP(*(.reset_entry))
    /* The trap vector table follows.  mtvec[1:0] = 01 (vectored) and
       bits [31:2] form the base, so the table just needs 4-byte
       alignment; 128 bytes keeps all 32 entries contiguous within
       one natural region. */
    . = ALIGN(128);
    KEEP(*(.vectors))
    KEEP(*(.resetHandler*))
    . = ALIGN(4);
    *(.text*)
    . = ALIGN(4);
    *(.fini*)
    . = ALIGN(4);
    *(.rodata*) 
    . = ALIGN(4);
    *(.srodata*) 
    . = ALIGN(4);
  } > RAM

/*  . = ALIGN(4);
  .copy.table (READONLY):
  {
    . = ALIGN(4);
    __copy_table_start__ = .;

    LONG (__etext)
    . = ALIGN(4);

    LONG (__data_start__)
    . = ALIGN(4);

    LONG ((__data_end__ - __data_start__) / 4)
    . = ALIGN(4); */

    /* Add each additional data section here */
    /*
    LONG (__etext2)
    LONG (__data2_start__)
    LONG ((__data2_end__ - __data2_start__) / 4)
    . = ALIGN(4);
    */
    /*. = ALIGN(4);
    __copy_table_end__ = .;
  } > ROM */

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
  } > RAM

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
    /* __Begin_Data__ = .; */
    __data_start__ = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    *(.sdata*)
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
    *(.bss*)
    . = ALIGN(4);
    *(.sbss*)
    . = ALIGN(4);
    *(COMMON)
    . = ALIGN(4);
    __bss_end__ = .;
  } > RAM 

  /* add any special bss sections here and add to .zero table above */

  /* __ebss is the end of last bss section */
  __ebss = .;
  __End_Bss__ = .;

  .stack (ORIGIN(RAM) + LENGTH(RAM) - __STACK_SIZE ) (COPY) :
  {
    . = ALIGN(4);
    __StackLimit = .;
    . = . + __STACK_SIZE;
    . = ALIGN(8);
    __StackTop = .;
  } > RAM
  PROVIDE(__freertos_irq_stack_top = __StackTop); 

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

  /* Task stacks and TCBs that opt in via __attribute__((section(".tasks")))
   * land in DDR rather than internal RAM.  NOLOAD because there is
   * nothing to copy at boot; the C runtime (or FreeRTOS port) writes
   * the initial stack frame at task creation. */
/*  .tasks (NOLOAD) :
  {
    . = ALIGN(8);
    __tasks_start__ = .;
    *(.tasks)
    *(.tasks.*)
    . = ALIGN(8);
    __tasks_end__ = .;
  } > TASKS */

  .heap (ORIGIN(HEAP)) (COPY):
  {
    . = ALIGN(4);
    end = .;
    PROVIDE( _HeapStart = . );   

    . += __HEAP_SIZE;
    
    __Heap_End = .;

    PROVIDE( _HeapTop = . );   

    . = ALIGN(4);

  /* /\*   PROVIDE(_end = .); *\/ */
  /* /\*   PROVIDE(__end = .); *\/ */
  /* /\*   PROVIDE(end = .); *\/ */
  } > HEAP
}
