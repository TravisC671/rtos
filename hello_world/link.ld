/* Linker script for Pyeatt's RISC-V SoC */
/* ITCM at 0x00000000 (128 KB) */

ENTRY(_vectors)

MEMORY
{
    ITCM (rx) : ORIGIN = 0x00000000, LENGTH = 128K
}

SECTIONS
{
    .vectors : { *(.vectors) } > ITCM
    .text : { *(.text*) } > ITCM
    .rodata : { *(.rodata*) } > ITCM
    /DISCARD/ : { *(.comment) *(.eh_frame) }
}
