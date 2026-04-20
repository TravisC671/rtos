set pagination off
set confirm off
set remote noack-packet off
target remote localhost:3334
printf "\n=== Test Progress ===\n"
printf "tests_run:    %d\n", *(int *)0x0800970c
printf "tests_passed: %d\n", *(int *)0x08009710
printf "tests_failed: %d\n", *(int *)0x08009714
printf "\n=== debug_corrupt ===\n"
printf "mepc:  0x%x\n", *(int *)0x080096dc
printf "stack: 0x%x\n", *(int *)0x080096e0
printf "tcb:   0x%x\n", *(int *)0x080096e4
printf "\n=== PC ===\n"
p/x $pc
detach
quit
