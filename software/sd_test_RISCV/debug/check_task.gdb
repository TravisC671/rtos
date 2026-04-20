set pagination off
set confirm off
set remote noack-packet off
target remote localhost:3334
printf "\n=== sd_test_TCB.pxTopOfStack ===\n"
p/x *(unsigned int *)&sd_test_TCB
printf "\n=== sd_test saved context ===\n"
set $tos = *(unsigned int *)&sd_test_TCB
x/31wx $tos
printf "\n=== sd_test saved mepc ===\n"
set $mepc_val = *(unsigned int *)$tos
p/x $mepc_val
printf "\n=== disasm at saved mepc ===\n"
x/8i $mepc_val
printf "\n=== INTC state ===\n"
printf "IER: "
x/1wx 0x41200008
printf "ISR: "
x/1wx 0x41200000
printf "MER: "
x/1wx 0x4120001c
printf "\n=== SD controller state ===\n"
printf "NormIntStat: "
x/1wx 0x44A40030
printf "Present State: "
x/1wx 0x44A40024
detach
quit
