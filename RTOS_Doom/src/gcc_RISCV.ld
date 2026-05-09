/*
 * Linker script for RISC-V / FreeRTOS / Doom on Nexys A7 DDR2
 *
 * Physical DDR2 map (128 MB total):
 *   0x10000000 – 0x11000000   16 MB  cached   (code + data + heap)
 *   0x11000000 – 0x18000000  112 MB  uncached  (framebuffer, audio, etc.)
 *
 * Cached window layout:
 *   0x10000000  .text / .rodata / .data / .bss     ~2 MB  (PRG_SIZE)
 *   0x10200000 – 0x10200FFF  stack (4 KB, grows down from 0x10201000)
 *   0x10201000 – 0x10FFFFFF  FreeRTOS heap (~14 MB, WAD lives here)
 */

/* ── sizes ──────────────────────────────────────────────────────── */
__STACK_SIZE = 0x00001000;   /*   4 KB – IRQ / main stack              */
__PRG_SIZE   = 0x00200000;   /*   2 MB – code+data+bss budget          */

/* ── derived addresses ──────────────────────────────────────────── */
__RAM_BASE   = 0x10000000;
__STACK_TOP  = __RAM_BASE + __PRG_SIZE;           /* 0x10200000 */
__STACK_BASE = __STACK_TOP - __STACK_SIZE;        /* 0x101FF000 */
__HEAP_BASE  = __STACK_TOP;                       /* 0x10200000 ... */
                                                  /* ... grows up    */
__HEAP_TOP   = 0x11000000;
__HEAP_SIZE  = __HEAP_TOP - __HEAP_BASE;          /* ~14 MB          */

/* ── uncached window (available for framebuffer / scratch) ──────── */
__UNCACHED_BASE = 0x11000000;
__UNCACHED_SIZE = 0x07000000;   /* 112 MB */

MEMORY
{
  /* 2 MB cached window: code, data, bss, stack */
  PRG   (rwx) : ORIGIN = __RAM_BASE,  LENGTH = __PRG_SIZE

  /* ~14 MB cached: FreeRTOS heap, WAD data */
  HEAP  (rwx) : ORIGIN = __HEAP_BASE, LENGTH = __HEAP_SIZE

  /* 112 MB uncached: framebuffer, DMA buffers, audio */
  UNCACHED (rwx) : ORIGIN = __UNCACHED_BASE, LENGTH = __UNCACHED_SIZE
}

ENTRY(_Reset_Handler)

SECTIONS
{
  /* ── reset vector ── must be at 0x10000000 ── */
  .reset_entry :
  {
    KEEP(*(.reset_entry))
  } > PRG

  /* ── interrupt vector table ── 128-byte aligned for vectored mtvec ── */
  . = ALIGN(128);
  .vectors :
  {
    KEEP(*(.vectors))
  } > PRG

  /* ── code ── */
  .text :
  {
    KEEP(*(.resetHandler*))
    . = ALIGN(4);
    *(.text .text.*)
    . = ALIGN(4);
    *(.fini .fini.*)
    . = ALIGN(4);
  } > PRG

  /* ── read-only data ── */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata .rodata.*)
    *(.srodata .srodata.*)
    . = ALIGN(4);
  } > PRG

  /* ── zero-table (used by startup to clear .bss) ── */
  .zero.table :
  {
    . = ALIGN(4);
    __zero_table_start__ = .;
    LONG(__bss_start__)
    LONG((__bss_end__ - __bss_start__) / 4)
    . = ALIGN(4);
    __zero_table_end__ = .;
  } > PRG

  __etext = .;   /* end of ROM-style content; LMA == VMA here */

  /* ── initialised data (already in RAM, no copy needed) ── */
  .data :
  {
    . = ALIGN(4);
    __data_start__ = .;
    *(.data .data.*)
    *(.sdata .sdata.*)
    . = ALIGN(4);
    __data_end__ = .;
  } > PRG

  /* ── zero-initialised data ── */
  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    __bss_start__ = .;
    *(.bss .bss.*)
    *(.sbss .sbss.*)
    *(COMMON)
    . = ALIGN(4);
    __bss_end__ = .;
  } > PRG

  __end_of_static = .;   /* first free byte after bss */

  /* ── stack ── placed at top of PRG region, grows downward ── */
  .stack (NOLOAD) :
  {
    . = ALIGN(8);
    __StackLimit = .;
    . += __STACK_SIZE;
    . = ALIGN(8);
    __StackTop = .;
  } > PRG
  /* FreeRTOS uses this symbol for the initial IRQ stack */
  PROVIDE(__freertos_irq_stack_top = __StackTop);

  /*
   * Sanity check: if static data + bss has grown past the stack base
   * the linker will error here.
   */
  ASSERT(__end_of_static <= __STACK_BASE,
         "ERROR: .bss overflows into stack – increase __PRG_SIZE")

  /* ── FreeRTOS heap (~14 MB, WAD data lives here) ── */
  .heap (NOLOAD) :
  {
    . = ALIGN(8);
    __HeapStart = .;
    PROVIDE(end   = .);   /* Newlib sbrk() starts here */
    PROVIDE(_end  = .);
    PROVIDE(__end = .);
    . += __HEAP_SIZE;
    . = ALIGN(8);
    __HeapEnd = .;
  } > HEAP
  PROVIDE(_HeapStart = __HeapStart);
  PROVIDE(_HeapTop   = __HeapEnd);

  /* ── uncached region symbols (no sections, just landmarks) ── */
  __uncached_start = ORIGIN(UNCACHED);
  __uncached_end   = ORIGIN(UNCACHED) + LENGTH(UNCACHED);
}