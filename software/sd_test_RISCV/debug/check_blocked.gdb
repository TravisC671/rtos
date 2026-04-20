set pagination off
set confirm off
set remote noack-packet off
target remote localhost:3334
set $tos = *(unsigned int *)&sd_test_TCB
printf "\n=== Saved mepc (where task yielded) ===\n"
set $mepc = *(unsigned int *)$tos
x/1i $mepc
printf "\n=== Saved ra ===\n"
set $ra = *(unsigned int *)($tos + 4)
x/4i $ra
printf "\n=== Saved s0 (fp) ===\n"
set $fp = *(unsigned int *)($tos + 20)
printf "s0 = 0x%x\n", $fp
printf "\n=== Caller return address (from frame) ===\n"
set $caller_ra = *(unsigned int *)($fp - 4)
x/4i $caller_ra
printf "\n=== Caller's caller ===\n"
set $fp2 = *(unsigned int *)($fp - 8)
printf "next fp = 0x%x\n", $fp2
set $caller2_ra = *(unsigned int *)($fp2 - 4)
x/4i $caller2_ra
printf "\n=== Caller's caller's caller ===\n"
set $fp3 = *(unsigned int *)($fp2 - 8)
printf "next fp = 0x%x\n", $fp3
set $caller3_ra = *(unsigned int *)($fp3 - 4)
x/4i $caller3_ra
printf "\n=== One more level ===\n"
set $fp4 = *(unsigned int *)($fp3 - 8)
printf "next fp = 0x%x\n", $fp4
set $caller4_ra = *(unsigned int *)($fp4 - 4)
x/4i $caller4_ra
detach
quit
