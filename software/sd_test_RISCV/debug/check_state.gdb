set pagination off
set confirm off
set remote noack-packet off
target remote localhost:3334
printf "\n=== PC ===\n"
p/x $pc
printf "\n=== debug_corrupt globals ===\n"
p/x debug_corrupt_mepc
p/x debug_corrupt_stack
p/x debug_corrupt_tcb
printf "\n=== pxCurrentTCB ===\n"
p/x pxCurrentTCB
printf "\n=== Where are we? ===\n"
bt 5
detach
quit
