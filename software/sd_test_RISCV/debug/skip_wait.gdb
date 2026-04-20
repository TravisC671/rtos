set pagination off
set confirm off
set remote noack-packet off
target remote localhost:3334
printf "\n=== Frames above wait_for_key ===\n"
set $tos = *(unsigned int *)&sd_test_TCB
set $fp = *(unsigned int *)($tos + 20)
set $fp2 = *(unsigned int *)($fp - 8)
set $fp3 = *(unsigned int *)($fp2 - 8)
set $fp4 = *(unsigned int *)($fp3 - 8)
set $caller4_ra = *(unsigned int *)($fp4 - 4)
printf "wait_for_key called from 0x%x\n", $caller4_ra
x/4i $caller4_ra
set $fp5 = *(unsigned int *)($fp4 - 8)
printf "next fp: 0x%x\n", $fp5
set $caller5_ra = *(unsigned int *)($fp5 - 4)
printf "next caller: 0x%x\n", $caller5_ra
x/4i $caller5_ra
detach
quit
