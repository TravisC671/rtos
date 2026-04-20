set pagination off
set confirm off
set remote noack-packet off
target remote localhost:3334
printf "\n=== sd_test saved context ===\n"
set $tos = *(unsigned int *)&sd_test_TCB
printf "pxTopOfStack = 0x%x\n", $tos
printf "saved mepc = 0x%x\n", *(unsigned int *)$tos
printf "saved ra   = 0x%x\n", *(unsigned int *)($tos+4)
printf "saved s0   = 0x%x\n", *(unsigned int *)($tos+20)
printf "saved s1   = 0x%x\n", *(unsigned int *)($tos+24)
printf "saved a0   = 0x%x\n", *(unsigned int *)($tos+28)
printf "saved a1   = 0x%x\n", *(unsigned int *)($tos+32)
printf "\n=== Stack frames (from saved s0 upward) ===\n"
set $fp = *(unsigned int *)($tos+20)
printf "frame 0: fp=0x%x\n", $fp
x/8wx $fp - 32
printf "\nframe 1:\n"
set $fp1 = *(unsigned int *)($fp - 4)
printf "caller ra at fp-4 = 0x%x\n", *(unsigned int *)($fp - 8)
printf "caller fp at fp-4 = 0x%x\n", $fp1
x/8wx $fp1 - 32
printf "\n=== What function is caller ra in? ===\n"
set $caller_ra = *(unsigned int *)($fp - 8)
x/4i $caller_ra - 4
detach
quit
