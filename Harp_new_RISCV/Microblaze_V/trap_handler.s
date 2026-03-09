# 0 "trap_handler.S"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 0 "<command-line>" 2
# 1 "trap_handler.S"
# 14 "trap_handler.S"
 .data
 .align 2
 .global RISCV_ExceptionVectorTable
RISCV_ExceptionVectorTable:
 .long _default_handler
 .long _default_handler
 .long _default_handler
 .long _default_handler
 .long _default_handler
 .long _default_handler
 .long _default_handler
 .long _default_handler
 .long _default_handler
 .long _default_handler
 .long _default_handler
 .long _default_handler

 .global RISCV_InterruptVectorTable
RISCV_InterruptVectorTable:
 .long _default_handler
 .long _default_handler
 .long _default_handler
 .long _default_handler
 .long _default_handler
 .long _default_handler
 .long _default_handler
 .long _default_handler
 .long _default_handler
 .long _default_handler
 .long _default_handler
 .long _default_handler
# 70 "trap_handler.S"
 .text
 .align 2
 .global _trap_handler
 .func _trap_handler
_trap_handler:

 addi sp, sp, -4*32

 sw x6, (6*4)(sp)
 sw x7, (7*4)(sp)
 sw x28, (28*4)(sp)
 sw x29, (29*4)(sp)
 sw x30, (30*4)(sp)
 csrr t1, mcause
 li t2, 6
 beq t1, t2, handle_store_misalign
 li t2, 4
 beq t1, t2, handle_load_misalign
 sw x1, (1*4)(sp)
 sw x3, (3*4)(sp)
 sw x4, (4*4)(sp)
 sw x5, (5*4)(sp)
 sw x8, (8*4)(sp)
 sw x9, (9*4)(sp)
 sw x10, (10*4)(sp)
 sw x11, (11*4)(sp)
 sw x12, (12*4)(sp)
 sw x13, (13*4)(sp)
 sw x14, (14*4)(sp)
 sw x15, (15*4)(sp)
 sw x16, (16*4)(sp)
 sw x17, (17*4)(sp)
 sw x18, (18*4)(sp)
 sw x19, (19*4)(sp)
 sw x20, (20*4)(sp)
 sw x21, (21*4)(sp)
 sw x22, (22*4)(sp)
 sw x23, (23*4)(sp)
 sw x24, (24*4)(sp)
 sw x25, (25*4)(sp)
 sw x26, (26*4)(sp)
 sw x27, (27*4)(sp)
 sw x31, (31*4)(sp)



 addi sp, sp, -33*8
 fsd f0, (0*8)(sp)
 fsd f1, (1*8)(sp)
 fsd f2, (2*8)(sp)
 fsd f3, (3*8)(sp)
 fsd f4, (4*8)(sp)
 fsd f5, (5*8)(sp)
 fsd f6, (6*8)(sp)
 fsd f7, (7*8)(sp)
 fsd f8, (8*8)(sp)
 fsd f9, (9*8)(sp)
 fsd f10, (10*8)(sp)
 fsd f11, (11*8)(sp)
 fsd f12, (12*8)(sp)
 fsd f13, (13*8)(sp)
 fsd f14, (14*8)(sp)
 fsd f15, (15*8)(sp)
 fsd f16, (16*8)(sp)
 fsd f17, (17*8)(sp)
 fsd f18, (18*8)(sp)
 fsd f19, (19*8)(sp)
 fsd f20, (20*8)(sp)
 fsd f21, (21*8)(sp)
 fsd f22, (22*8)(sp)
 fsd f23, (23*8)(sp)
 fsd f24, (24*8)(sp)
 fsd f25, (25*8)(sp)
 fsd f26, (26*8)(sp)
 fsd f27, (27*8)(sp)
 fsd f28, (28*8)(sp)
 fsd f29, (29*8)(sp)
 fsd f30, (30*8)(sp)
 fsd f31, (31*8)(sp)
 csrr t2, fcsr
 sw t2, (8*32)(sp)



 bltz t1, is_interrupt

 la t2, RISCV_ExceptionVectorTable
 j is_exception
is_interrupt:
 la t2, RISCV_InterruptVectorTable
is_exception:
        slli t1, t1, 4
 add t2, t2, t1
 lw t2, 0(t2)
 jalr t2






 lw t2, (8*32)(sp)
 csrw fcsr, t2
 fld f31, (31*8)(sp)
 fld f30, (30*8)(sp)
 fld f29, (29*8)(sp)
 fld f28, (28*8)(sp)
 fld f27, (27*8)(sp)
 fld f26, (26*8)(sp)
 fld f25, (25*8)(sp)
 fld f24, (24*8)(sp)
 fld f23, (23*8)(sp)
 fld f22, (22*8)(sp)
 fld f21, (21*8)(sp)
 fld f20, (20*8)(sp)
 fld f19, (19*8)(sp)
 fld f18, (18*8)(sp)
 fld f17, (17*8)(sp)
 fld f16, (16*8)(sp)
 fld f15, (15*8)(sp)
 fld f14, (14*8)(sp)
 fld f13, (13*8)(sp)
 fld f12, (12*8)(sp)
 fld f11, (11*8)(sp)
 fld f10, (10*8)(sp)
 fld f9, (9*8)(sp)
 fld f8, (8*8)(sp)
 fld f7, (7*8)(sp)
 fld f6, (6*8)(sp)
 fld f5, (5*8)(sp)
 fld f4, (4*8)(sp)
 fld f3, (3*8)(sp)
 fld f2, (2*8)(sp)
 fld f1, (1*8)(sp)
 fld f0, (0*8)(sp)
 addi sp, sp, 33*8

 lw x31, (31*4)(sp)
 lw x27, (27*4)(sp)
 lw x26, (26*4)(sp)
 lw x25, (25*4)(sp)
 lw x24, (24*4)(sp)
 lw x23, (23*4)(sp)
 lw x22, (22*4)(sp)
 lw x21, (21*4)(sp)
 lw x20, (20*4)(sp)
 lw x19, (19*4)(sp)
 lw x18, (18*4)(sp)
 lw x17, (17*4)(sp)
 lw x16, (16*4)(sp)
 lw x15, (15*4)(sp)
 lw x14, (14*4)(sp)
 lw x13, (13*4)(sp)
 lw x12, (12*4)(sp)
 lw x11, (11*4)(sp)
 lw x10, (10*4)(sp)
 lw x9, (9*4)(sp)
 lw x8, (8*4)(sp)
 lw x5, (5*4)(sp)
 lw x4, (4*4)(sp)
 lw x3, (3*4)(sp)
 lw x2, (2*4)(sp)
 lw x1, (1*4)(sp)
 lw x0, (0*4)(sp)

misalign_done:
 lw x30, (30*4)(sp)
 lw x29, (29*4)(sp)
 lw x28, (28*4)(sp)
 lw x7, (7*4)(sp)
 lw x6, (6*4)(sp)
 addi sp, sp, 4*32
 mret
 .endfunc
