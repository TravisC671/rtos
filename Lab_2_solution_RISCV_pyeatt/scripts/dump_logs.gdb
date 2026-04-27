set pagination off
target remote :3333
file bin/Lab_2_solution_RISCV_pyeatt.elf

printf "===== Live state =====\n"
info reg pc mstatus mepc mcause mtvec mip mie
print idle_hook_count
printf "idle_hook_last_mstatus = 0x%08x\n", idle_hook_last_mstatus
printf "save_entered_count     = %d\n", save_entered_count
printf "pxCurrentTCB = 0x%08x\n", pxCurrentTCB
printf "  hello_TCB  = 0x%08x\n", &hello_TCB

printf "\n===== mstatus_log (per-restore) =====\n"
set $rl = (unsigned int*)mstatus_log
printf "log_idx (bytes) = 0x%08x  (= %d entries)\n", $rl[0], $rl[0]/8
set $i = 0
set $stop = $rl[0]/8
if $stop > 64
  set $stop = 64
end
while $i < $stop
  printf "  R[%2d]  mstatus=0x%08x  mcause=0x%08x\n", $i, $rl[1+2*$i], $rl[2+2*$i]
  set $i = $i + 1
end

printf "\n===== save_log (per-trap-entry) =====\n"
set $sl = (unsigned int*)save_log
printf "save_idx (bytes) = 0x%08x  (= %d entries)\n", $sl[0], $sl[0]/16
set $i = 0
set $stop = $sl[0]/16
if $stop > 32
  set $stop = 32
end
while $i < $stop
  printf "  S[%2d]  mepc=0x%08x  mst=0x%08x  mcause=0x%08x  TCB=0x%08x\n", \
         $i, $sl[1+4*$i], $sl[2+4*$i], $sl[3+4*$i], $sl[4+4*$i]
  set $i = $i + 1
end

printf "\n===== Saved frame at pxCurrentTCB.ptos =====\n"
set $tcb = pxCurrentTCB
set $sp_saved = *(unsigned int*)$tcb
printf "  ptos = 0x%08x\n", $sp_saved
set $f = (unsigned int*)$sp_saved
printf "  slot 0  (mepc)    = 0x%08x\n", $f[0]
printf "  slot 12 (a5)      = 0x%08x\n", $f[12]
printf "  slot 29 (critnest)= 0x%08x\n", $f[29]
printf "  slot 30 (mstatus) = 0x%08x\n", $f[30]

detach
quit
