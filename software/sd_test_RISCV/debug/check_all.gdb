set pagination off
set confirm off
set remote noack-packet off
target remote localhost:3334
printf "\n=== PC ===\n"
p/x $pc
printf "\n=== Key Registers ===\n"
printf "a0 = 0x%x\n", $a0
printf "a1 = 0x%x\n", $a1
printf "t0 = 0x%x\n", $t0
printf "t1 = 0x%x\n", $t1
printf "sp = 0x%x\n", $sp
printf "ra = 0x%x\n", $ra
printf "\n=== Test Progress ===\n"
printf "tests_run:     %d\n", tests_run
printf "tests_passed:  %d\n", tests_passed
printf "tests_failed:  %d\n", tests_failed
printf "tests_skipped: %d\n", tests_skipped
printf "\n=== Backtrace ===\n"
bt 5
detach
quit
