set pagination off
set confirm off
set remote noack-packet off
target remote localhost:3334
printf "\n=== PC ===\n"
p/x $pc
printf "\n=== Registers at spin point ===\n"
printf "a0 (mcause)  = 0x%x\n", $a0
printf "a1 (mepc)    = 0x%x\n", $a1
printf "sp (task sp) = 0x%x\n", $sp
printf "ra           = 0x%x\n", $ra
printf "t0           = 0x%x\n", $t0
printf "\n=== debug_corrupt globals ===\n"
printf "mepc:  0x%x\n", *(int *)0x080096dc
printf "stack: 0x%x\n", *(int *)0x080096e0
printf "tcb:   0x%x\n", *(int *)0x080096e4
printf "\n=== Test Progress ===\n"
printf "tests_run:    %d\n", *(int *)0x0800970c
printf "tests_passed: %d\n", *(int *)0x08009710
printf "tests_failed: %d\n", *(int *)0x08009714
printf "\n=== pxCurrentTCB ===\n"
p/x pxCurrentTCB
printf "\n=== Saved context at pxTopOfStack (just saved by trap entry) ===\n"
x/31wx $sp
printf "\n=== INTC state ===\n"
printf "ISR: "
x/1wx 0x41200000
printf "IER: "
x/1wx 0x41200008
printf "IVR: "
x/1wx 0x41200018
printf "\n=== What's at the corrupt mepc address? ===\n"
set $bad_mepc = $a1
x/4wx $bad_mepc
x/4i $bad_mepc
detach
quit
