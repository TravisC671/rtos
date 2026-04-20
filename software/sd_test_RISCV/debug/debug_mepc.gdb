set pagination off
set confirm off
set remote noack-packet off
target remote localhost:3334
load
set $pc = 0
# Clear debug globals (in BSS, might retain stale values after load)
set {int}0x080096dc = 0
set {int}0x080096e0 = 0
set {int}0x080096e4 = 0
break vApplicationStackOverflowHook
break freertos_risc_v_application_exception_handler
continue
