# Hello World for Pyeatt's RISC-V SoC
#
# Prints "Hello, World!\r\n" to UART0 at 57600 baud, then halts.
# Includes a 32-entry vectored interrupt table at address 0x00.
#
# Xilinx AXI UART16550 registers at base + 0x1000:
#   THR  = base + 0x1000  (write: transmit data, 8 bits)
#   DLL  = base + 0x1000  (write: divisor low byte, when DLAB=1)
#   DLM  = base + 0x1004  (write: divisor high byte, when DLAB=1)
#   FCR  = base + 0x1008  (write: FIFO control)
#   LCR  = base + 0x100C  (write: line control)
#   LSR  = base + 0x1014  (read:  line status)
#
# Note: DLL/DLM are 8-bit registers. For divisors > 255, the value
# must be split into low and high bytes written separately.
#
# UART IP base = 0x44A10000  =>  register base = 0x44A11000

.equ UART_BASE, 0x44A11000
.equ UART_THR,  0x000       # offset from UART_BASE
.equ UART_DLL,  0x000
.equ UART_DLM,  0x004
.equ UART_FCR,  0x008
.equ UART_LCR,  0x00C
.equ UART_LSR,  0x014

# =========================================================================
# Vectored interrupt table (32 entries x 4 bytes = 128 bytes at address 0)
# Entry 0:     exception handler (also reset vector)
# Entries 1-31: interrupt handlers (standard RISC-V cause codes)
#   Cause  3: MSI  (machine software interrupt)
#   Cause  7: MTI  (machine timer interrupt)
#   Cause 11: MEI  (machine external interrupt)
#   Causes 16-31: platform IRQ[0]-IRQ[15]
# =========================================================================
.section .vectors, "ax"
.globl _vectors
_vectors:
    j       _start          # 0x00: exception / reset
    j       _unhandled      # 0x04: reserved (cause 1)
    j       _unhandled      # 0x08: reserved (cause 2)
    j       _unhandled      # 0x0C: MSI - machine software interrupt
    j       _unhandled      # 0x10: reserved (cause 4)
    j       _unhandled      # 0x14: reserved (cause 5)
    j       _unhandled      # 0x18: reserved (cause 6)
    j       _unhandled      # 0x1C: MTI - machine timer interrupt
    j       _unhandled      # 0x20: reserved (cause 8)
    j       _unhandled      # 0x24: reserved (cause 9)
    j       _unhandled      # 0x28: reserved (cause 10)
    j       _unhandled      # 0x2C: MEI - machine external interrupt
    j       _unhandled      # 0x30: reserved (cause 12)
    j       _unhandled      # 0x34: reserved (cause 13)
    j       _unhandled      # 0x38: reserved (cause 14)
    j       _unhandled      # 0x3C: reserved (cause 15)
    j       _unhandled      # 0x40: platform IRQ[0]
    j       _unhandled      # 0x44: platform IRQ[1]
    j       _unhandled      # 0x48: platform IRQ[2]
    j       _unhandled      # 0x4C: platform IRQ[3]
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

# =========================================================================
# Main program
# =========================================================================
.section .text

_start:
main:	
    # ---- UART init ----
    # Load UART register base into s0
    lui     s0, %hi(UART_BASE)
    addi    s0, s0, %lo(UART_BASE)

    # Reset FIFOs first (FCR = 0x07: enable + reset TX/RX FIFOs).
    # This clears any stale data from a previous run (e.g. after
    # a debugger halt/resume without a full hardware reset).
    li      t0, 0x07
    sw      t0, UART_FCR(s0)

    # Set DLAB=1, 8N1  (LCR = 0x83)
    li      t0, 0x83
    sw      t0, UART_LCR(s0)

    # Divisor = 108 (0x6C) for 57600 baud @ 100 MHz
    li      t0, 108
    sw      t0, UART_DLL(s0)
    sw      zero, UART_DLM(s0)

    # Clear DLAB, keep 8N1  (LCR = 0x03)
    li      t0, 0x03
    sw      t0, UART_LCR(s0)

    # Re-enable FIFOs after baud rate setup (FCR = 0x01)
    li      t0, 0x01
    sw      t0, UART_FCR(s0)

    # ---- Print message ----
    la      a0, message
    jal     ra, puts

    # ---- Halt ----
halt:
    j       halt

# ---------------------------------------------------------
# puts: print null-terminated string at a0
# clobbers: t0, t1
# ---------------------------------------------------------
puts:
    lbu     t0, 0(a0)           # load next char
    beqz    t0, puts_done       # null => return
puts_wait:
    lw      t1, UART_LSR(s0)    # read LSR
    andi    t1, t1, 0x20        # THRE bit (bit 5)
    beqz    t1, puts_wait       # spin until ready
    sw      t0, UART_THR(s0)    # write char
    addi    a0, a0, 1
    j       puts
puts_done:
    ret

# ---------------------------------------------------------
# Unhandled interrupt/exception: spin forever
# ---------------------------------------------------------
_unhandled:
    j       _unhandled

.section .rodata
message:
    .asciz "Hello, World!\r\n"
