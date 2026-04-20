set pagination off
set confirm off
set remote noack-packet off
target remote localhost:3334
printf "\n=== PC ===\n"
p/x $pc
printf "\n=== Key registers ===\n"
printf "ra  = 0x%x\n", $ra
printf "sp  = 0x%x\n", $sp
printf "t0  = 0x%x\n", $t0
printf "t1  = 0x%x\n", $t1
printf "t2  = 0x%x\n", $t2
printf "\n=== debug_corrupt globals ===\n"
printf "mepc:  0x%x\n", *(int *)0x080096dc
printf "stack: 0x%x\n", *(int *)0x080096e0
printf "tcb:   0x%x\n", *(int *)0x080096e4
printf "\n=== pxCurrentTCB ===\n"
p/x pxCurrentTCB
printf "\n=== Saved context at pxTopOfStack ===\n"
set $tos = *(unsigned int *)pxCurrentTCB
printf "pxTopOfStack = 0x%x\n", $tos
x/31wx $tos
printf "\n=== Stack (sp) ===\n"
x/16wx $sp
printf "\n=== INTC state ===\n"
printf "ISR: "
x/1wx 0x41200000
printf "IER: "
x/1wx 0x41200008
printf "IVR: "
x/1wx 0x41200018
printf "MER: "
x/1wx 0x4120001c
printf "\n=== IVAR table ===\n"
x/10wx 0x41200100
detach
quit
