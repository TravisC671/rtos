/******************************************************************************
* riscv_enable_icache()
*
*    Enable the instruction cache.  Unlike classic MicroBlaze (which has
*    DCE/ICE bits in the MSR), MicroBlaze V has NO cache enable bits in
*    mstatus or any other CSR (confirmed: UG1629 v2025.2, Table 7,
*    pages 37-38).
*
*    Caches are always active when C_USE_ICACHE=1.  After reset, hardware
*    automatically invalidates all cache lines (UG1629 page 69).  The
*    official Xilinx BSP (standalone_v9_3) also defines Xil_ICacheEnable()
*    as an empty macro in xil_cache.h for the same reason.
*
*    This function is a no-op stub, called from startup for compatibility.
*
******************************************************************************/

#include "xpseudo_asm.h"
#include "xparameters.h"

.global	riscv_enable_icache
.section .text
.align 2
.type riscv_enable_icache, @function

riscv_enable_icache:
	ret
.end riscv_enable_icache
