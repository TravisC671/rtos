set pagination off
set confirm off
file bin/Lab_2_solution_RISCV_pyeatt.elf
target remote :3333

# Reset and download ELF.  monitor commands go to the J-Link gdbserver.
monitor reset
load
monitor reset

# Verify .text is intact at 0xc0 right after load.  If this is already
# corrupted, the problem is in load/init, not at runtime.
printf "After load:\n"
printf "  *(0xc0)  = 0x%08x  (expect 0x1580006f = j _unhandled)\n", *(unsigned int*)0xc0
printf "  *(0x33a4) = 0x%08x  (expect 0x01010113 = addi sp,sp,16)\n", *(unsigned int*)0x33a4

# Watch for any WRITE to 0xc0 (the first corrupting access).
watch *(unsigned int*)0xc0

printf "\nWatchpoint armed at *(0xc0).  Resuming.\n"
continue

# Once halted, dump everything useful.
printf "\n========== Watchpoint hit! ==========\n"
info reg pc mstatus mepc mcause
printf "ra = 0x%08x\n", $ra
printf "sp = 0x%08x\n", $sp
printf "s0 = 0x%08x\n", $s0
printf "a0 = 0x%08x  a1 = 0x%08x\n", $a0, $a1
printf "a4 = 0x%08x  a5 = 0x%08x\n", $a4, $a5

printf "\n8 instructions before PC and at PC:\n"
x/12i $pc - 32

printf "\n*(0xc0) is now: 0x%08x\n", *(unsigned int*)0xc0
printf "*(0x33a4) is now: 0x%08x\n", *(unsigned int*)0x33a4

printf "\nStack walk:\n"
backtrace 6

detach
quit
