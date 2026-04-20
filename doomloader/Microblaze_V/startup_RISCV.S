/* Set up the vector table */


/*  The mtvec register selects the vectored interrupt mode. The LSB is
    a mode bit selects the mode. Setting the LSB to 1 enables vectored
    mode. In this mode, mtvec will point to a table of
    instructions. (not a single routine)

    - Only interrupts are vectored. Synchronous exceptions are NOT vectored.

    - Each entry in the table is an instruction, NOT an address.

    - The offset in the table is 4 x the numbered cause in the mcause
      register (this is also the bit position from the mip register).

     - Entry 0 is always the synchronous exception vector.

     - Each entry is allocated 4 bytes.
	- However, if the following entry is not used a routine longer
          than 4 bytes could be implemented.

	- As user/supervisor/machine mode interrupts are interleaved,
          if only one privilege is used a small 12 byte routine can be
          implemented.

    There are separate tables for supervisor (stvec) and user (utvec)
    privileges.  Supervisor mode and user mode are optional.

*/
	
.section .vectors
        .align   2
        .globl   __Vectors
        .globl   __Vectors_End
        .globl   __Vectors_Size

__Vectors:
    j       _Reset_Handler          # 0x00: exception / reset
    j       freertos_risc_v_trap_handler   # 0x04: reserved (cause 1)
    j       _unhandled      # 0x08: reserved (cause 2)
    j       freertos_risc_v_trap_handler      # 0x0C: MSI - machine software interrupt
    j       _unhandled      # 0x10: reserved (cause 4)
    j       _unhandled      # 0x14: reserved (cause 5)
    j       _unhandled      # 0x18: reserved (cause 6)
    j       mtime_tick_handler      # 0x1C: MTI - machine timer interrupt
    j       _unhandled      # 0x20: reserved (cause 8)
    j       _unhandled      # 0x24: reserved (cause 9)
    j       _unhandled      # 0x28: reserved (cause 10)
    j       freertos_risc_v_trap_handler      # 0x2C: MEI - machine external interrupt
    j       _unhandled      # 0x30: reserved (cause 12)
    j       _unhandled      # 0x34: reserved (cause 13)
    j       _unhandled      # 0x38: reserved (cause 14)
    j       _unhandled      # 0x3C: reserved (cause 15)
    j       UART0_handler   # 0x40: platform IRQ[0]
    j       UART1_handler   # 0x44: platform IRQ[1]
    j       AXI_TIMER_0_ISR # 0x48: platform IRQ[2]
    j       AXI_TIMER_1_ISR # 0x4C: platform IRQ[3]
    j       _unhandled      # 0x50: platform IRQ[4]
    j       _unhandled      # 0x54: platform IRQ[5]
    j       _unhandled      # 0x58: platform IRQ[6]
    j       _unhandled      # 0x5C: platform IRQ[7]
    j       _unhandled      # 0x60: platform IRQ[8]
    j       _unhandled      # 0x64: platform IRQ[9]
    j       _unhandled      # 0x68: platform IRQ[10]
    j       _unhandled      # 0x6C: platform IRQ[11]
    j       _unhandled      # 0x70: platform IRQ[12]
    j       _unhandled      # 0x74: platform IRQ[13]
    j       _unhandled      # 0x78: platform IRQ[14]
    j       _unhandled      # 0x7C: platform IRQ[15]
__Vectors_End:
        .equ     __Vectors_Size, __Vectors_End - __Vectors
        .size    __Vectors, . - __Vectors


        .section .init
        .func    _Reset_Handler
        .global  _Reset_Handler
_Reset_Handler:
	/* Disable machine interrupts immediately.  After a GDB reload,
	   mstatus.MIE is still 1 from the previous run and the INTC
	   retains stale IER/MER, so interrupts fire during startup
	   before BSS is zeroed or main() runs → crash.  MIE is
	   re-enabled by FreeRTOS when the scheduler starts. */
        csrw mie, zero
        csrci mstatus, 0x8
        csrw mip, zero

        li      t0, 0x01
        //csrw    mtvec, t0
        
        //csrc    mie, 0x8
        
	/* Load the mtvec CSR with the address of the trap handler */
	//la	t0, _trap_handler
	//la	t0, freertos_risc_v_trap_handler
	//csrw	mtvec, t0
	
	/* Initialize processor registers to 0 */
	addi    x1,x0,0
        addi    x3,x0,0
        addi    x4,x0,0
        addi    x5,x0,0
        addi    x6,x0,0
        addi    x7,x0,0
        addi    x8,x0,0
        addi    x9,x0,0
        addi    x10,x0,0
        addi    x11,x0,0
        addi    x12,x0,0
        addi    x13,x0,0
        addi    x14,x0,0
        addi    x15,x0,0
        addi    x16,x0,0
        addi    x17,x0,0
        addi    x18,x0,0
        addi    x19,x0,0
        addi    x20,x0,0
        addi    x21,x0,0
        addi    x22,x0,0
        addi    x23,x0,0
        addi    x24,x0,0
        addi    x25,x0,0
        addi    x26,x0,0
        addi    x27,x0,0
        addi    x28,x0,0
        addi    x29,x0,0
        addi    x30,x0,0
        addi    x31,x0,0

/* Clear the floating point registers, if they exist*/
#if defined(XPAR_MICROBLAZE_RISCV_USE_FPU)
#if(XPAR_MICROBLAZE_RISCV_USE_FPU == 2)
	/* single precision FPU */
        fmv.d.x f0, x0
        fmv.d.x f1, x0
        fmv.d.x f2, x0
        fmv.d.x f3, x0
        fmv.d.x f4, x0
        fmv.d.x f5, x0
        fmv.d.x f6, x0
        fmv.d.x f7, x0
        fmv.d.x f8, x0
        fmv.d.x f9, x0
        fmv.d.x f10, x0
        fmv.d.x f11, x0
        fmv.d.x f12, x0
        fmv.d.x f13, x0
        fmv.d.x f14, x0
        fmv.d.x f15, x0
        fmv.d.x f16, x0
        fmv.d.x f17, x0
        fmv.d.x f18, x0
        fmv.d.x f19, x0
        fmv.d.x f20, x0
        fmv.d.x f21, x0
        fmv.d.x f22, x0
        fmv.d.x f23, x0
        fmv.d.x f24, x0
        fmv.d.x f25, x0
        fmv.d.x f26, x0
        fmv.d.x f27, x0
        fmv.d.x f28, x0
        fmv.d.x f29, x0
        fmv.d.x f30, x0
        fmv.d.x f31, x0
#endif
#if(XPAR_MICROBLAZE_RISCV_USE_FPU == 1)
	/* double precision FPU */
        fmv.w.x f0, x0
        fmv.w.x f1, x0
        fmv.w.x f2, x0
        fmv.w.x f3, x0
        fmv.w.x f4, x0
        fmv.w.x f5, x0
        fmv.w.x f6, x0
        fmv.w.x f7, x0
        fmv.w.x f8, x0
        fmv.w.x f9, x0
        fmv.w.x f10, x0
        fmv.w.x f11, x0
        fmv.w.x f12, x0
        fmv.w.x f13, x0
        fmv.w.x f14, x0
        fmv.w.x f15, x0
        fmv.w.x f16, x0
        fmv.w.x f17, x0
        fmv.w.x f18, x0
        fmv.w.x f19, x0
        fmv.w.x f20, x0
        fmv.w.x f21, x0
        fmv.w.x f22, x0
        fmv.w.x f23, x0
        fmv.w.x f24, x0
        fmv.w.x f25, x0
        fmv.w.x f26, x0
        fmv.w.x f27, x0
        fmv.w.x f28, x0
        fmv.w.x f29, x0
        fmv.w.x f30, x0
        fmv.w.x f31, x0
#endif
#endif
	
        // Set the stack pointer to value given by linker script
        la      sp,__StackTop

        // clear the bss section
	la      t0,__bss_start__ // get address where .bss starts
        la      t1,__bss_end__   // get address where .bss ends
	bge	t0,t1,movedata   // if there is no .bss, skip the loop
        mv      t3,zero
bssloop:sw      t3,(t0)
	addi	t0,t0,4
        blt     t0,t1,bssloop

        // copy initialized data from ROM to RAM
movedata:	
        la     t0,__copy_table_start__  
        la     t1,__copy_table_end__
copyloop:
        bge     t0,t1,call_main // exit if not more entries in the table
        lw      t2,(t0)       // get source address of next chunk
	addi    t0,t0,4
        lw      t4,(t0)       // get target address of next chunk
	addi    t0,t0,4
        lw      t3,(t0)       // get number of words to move
	addi    t0,t0,4

ctloop: lw      t5,(t2)      // load word from this chunk
	addi    t2,t2,4
        sw      t5,(t4)      // store word from this chunk to the target addr
	addi    t4,t4,4
        addi    t3,t3,-1     // subtract 
        bgtz    t3,ctloop    // if not done, keep going.
        j       copyloop     // top of loop checks to see if we are done.

call_main:
        jal   main
hang:   j     hang
	.endfunc


	
