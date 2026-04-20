set pagination off
set confirm off
set remote noack-packet off
target remote localhost:3334
load
set $pc = 0
detach
quit
