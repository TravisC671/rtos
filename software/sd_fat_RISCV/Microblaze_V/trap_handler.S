
/* This file contains the trap handler.
	
   The trap handler looks at the mcause CSR and determines which
   handler to call.  It uses two tables to hold the defined exceptions
   and interrupts. The RISCV standard defines 12 exceptions and 12
   interrupts. Any unimplemented handlers are given a default handler.
   */


/*
 * Exception and interrupt vector tables
*/
	.data
	.align	2
	.global RISCV_ExceptionVectorTable
RISCV_ExceptionVectorTable:
	.long	_default_handler	/* Instruction access misaligned */
	.long	_default_handler	/* Instruction access fault */
	.long	_default_handler	/* Illegal instruction */
	.long	_default_handler	/* Breakpoint */
	.long	_default_handler	/* Load address misaligned */
	.long	_default_handler	/* Load access fault */
	.long	_default_handler	/* Store/AMO address misaligned */
	.long	_default_handler	/* Store/AMO access fault */
	.long	_default_handler	/* Unimplemented */
	.long	_default_handler	/* Unimplemented */
	.long	_default_handler	/* Reserved */
	.long	_default_handler	/* Environment call from M-mode */

	.global RISCV_InterruptVectorTable
RISCV_InterruptVectorTable:
	.long	_default_handler	/* Unimplemented */
	.long	_default_handler	/* Unimplemented */
	.long	_default_handler	/* Reserved for future standard use */
	.long	_default_handler	/* Machine software interrupt */
	.long	_default_handler	/* Unimplemented */
	.long	_default_handler	/* Unimplemented */
	.long	_default_handler	/* Reserved for future standard use */
	.long	_default_handler	/* Machine timer interrupt */
	.long	_default_handler	/* Unimplemented */
	.long	_default_handler	/* Unimplemented */
	.long	_default_handler	/* Reserved for future standard use */
	.long	_default_handler	/* Machine external interrupt */
	



#define PUSHINT(regnum)	sw x ## regnum, (regnum*4)(sp)
#define POPINT(regnum)	lw x ## regnum, (regnum*4)(sp)
	
#if defined(XPAR_MICROBLAZE_RISCV_USE_FPU)
#if(XPAR_MICROBLAZE_RISCV_USE_FPU == 1)
	/* if we have a single precision FP unit */
	/* Allocate space on the stack to store all of the registers */
#define PUSHFP(regnum)	fsw f ## regnum, (regnum*4)(sp)
#define POPFP(regnum)	flw f ## regnum, (regnum*4)(sp)
#define FPREGSIZE 4
#define FPSTACKSPACE 33*4
#endif
#if(XPAR_MICROBLAZE_RISCV_USE_FPU == 2)
#define PUSHFP(regnum)	fsd f ## regnum, (regnum*8)(sp)
#define POPFP(regnum)	fld f ## regnum, (regnum*8)(sp)
#define FPREGSIZE 8
#define FPSTACKSPACE 33*8
#endif
#endif

	
	.text
	.align 	2
	.global	_default_handler
	.func	_default_handler
_default_handler:
	j	_default_handler
	.endfunc

	
	.align 	2
	.global _trap_handler
	.func	_trap_handler
_trap_handler:
	/* Allocate space on the stack to store all of the registers */
	addi	sp, sp, -4*32	
	/* Save some temp regs */
	PUSHINT(6)			/* Save t1 = x6 */
	PUSHINT(7)			/* Save t2 = x7 */
	PUSHINT(28)			/* Save t3 = x28 */
	PUSHINT(29)			/* Save t4 = x29 */
	PUSHINT(30)			/* Save t5 = x30 */
	// csrr	t1, mcause		/* Get the exception cause */
	// li	t2, 6			/* If it is store address misaligned */
	// beq	t1, t2, handle_store_misalign /* handle it */
	// li	t2, 4			/* If it is load address misaligned */
	// beq	t1, t2, handle_load_misalign  /* handle it */
	PUSHINT(1)
	PUSHINT(3)
	PUSHINT(4)
	PUSHINT(5)
	PUSHINT(8)
	PUSHINT(9)
	PUSHINT(10)
	PUSHINT(11)
	PUSHINT(12)
	PUSHINT(13)
	PUSHINT(14)
	PUSHINT(15)
	PUSHINT(16)
	PUSHINT(17)
	PUSHINT(18)
	PUSHINT(19)
	PUSHINT(20)
	PUSHINT(21)
	PUSHINT(22)
	PUSHINT(23)
	PUSHINT(24)
	PUSHINT(25)
	PUSHINT(26)
	PUSHINT(27)
	PUSHINT(31)
#if defined(XPAR_MICROBLAZE_RISCV_USE_FPU) && (XPAR_MICROBLAZE_RISCV_USE_FPU > 0)
	/* If we have an FP unit, allocate space on the stack to store
	   all of the registers and push them. */
	addi	sp, sp, -FPSTACKSPACE
	PUSHFP(0)					/* Save f0 to f31 */
	PUSHFP(1)
	PUSHFP(2)
	PUSHFP(3)
	PUSHFP(4)
	PUSHFP(5)
	PUSHFP(6)
	PUSHFP(7)
	PUSHFP(8)
	PUSHFP(9)
	PUSHFP(10)
	PUSHFP(11)
	PUSHFP(12)
	PUSHFP(13)
	PUSHFP(14)
	PUSHFP(15)
	PUSHFP(16)
	PUSHFP(17)
	PUSHFP(18)
	PUSHFP(19)
	PUSHFP(20)
	PUSHFP(21)
	PUSHFP(22)
	PUSHFP(23)
	PUSHFP(24)
	PUSHFP(25)
	PUSHFP(26)
	PUSHFP(27)
	PUSHFP(28)
	PUSHFP(29)
	PUSHFP(30)
	PUSHFP(31)
	csrr	t2, fcsr			/* Save fcsr */
	sw	t2, (FPREGSIZE*32)(sp)
#endif
	/* Load the address of the appropriate vector table */
	/* If top bit of cause is not set */
	bltz	t1, is_interrupt
	/* then Load exception vector table base address */
	la	t2, RISCV_ExceptionVectorTable	
	j	is_exception
is_interrupt:	
	la	t2, RISCV_InterruptVectorTable	
is_exception:
        slli	t1, t1, 4	/* Calculate offset to the vector */
	add	t2, t2, t1	/* Get pointer to the vector */
	lw	t2, 0(t2)	/* Load the vector */
	jalr	t2		/* Jump to handler */
	

#if defined(XPAR_MICROBLAZE_RISCV_USE_FPU) && (XPAR_MICROBLAZE_RISCV_USE_FPU > 0)
	/* If we have an FP unit, pop the registers and deallocate
	   space on the stack.
	*/
	lw	t2, (FPREGSIZE*32)(sp)
	csrw	fcsr, t2			/* Restore fcsr */
	POPFP(31)
	POPFP(30)
	POPFP(29)
	POPFP(28)
	POPFP(27)
	POPFP(26)
	POPFP(25)
	POPFP(24)
	POPFP(23)
	POPFP(22)
	POPFP(21)
	POPFP(20)
	POPFP(19)
	POPFP(18)
	POPFP(17)
	POPFP(16)
	POPFP(15)
	POPFP(14)
	POPFP(13)
	POPFP(12)
	POPFP(11)
	POPFP(10)
	POPFP(9)
	POPFP(8)
	POPFP(7)
	POPFP(6)
	POPFP(5)
	POPFP(4)
	POPFP(3)
	POPFP(2)
	POPFP(1)
	POPFP(0)
	addi	sp, sp, FPSTACKSPACE
#endif
	POPINT(31)
	POPINT(27)
	POPINT(26)
	POPINT(25)
	POPINT(24)
	POPINT(23)
	POPINT(22)
	POPINT(21)
	POPINT(20)
	POPINT(19)
	POPINT(18)
	POPINT(17)
	POPINT(16)
	POPINT(15)
	POPINT(14)
	POPINT(13)
	POPINT(12)
	POPINT(11)
	POPINT(10)
	POPINT(9)
	POPINT(8)
	POPINT(5)
	POPINT(4)
	POPINT(3)
	POPINT(2)
	POPINT(1)
	POPINT(0)
/* if it was an alignment exception, the function will return here */	
misalign_done:	 
	POPINT(30)
	POPINT(29)
	POPINT(28)
	POPINT(7)
	POPINT(6)
	addi	sp, sp, 4*32	
	mret
	.endfunc
