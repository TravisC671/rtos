// FreeRTOSConfig.h -- RISC-V (MicroBlaze V) configuration for sd_test

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

// RISC-V MTIME configuration
// MTIME interrupt goes through AXI INTC (cause 11), not direct (cause 7).
#define configMTIME_BASE_ADDRESS     (0x44A00000)
#define configMTIMECMP_BASE_ADDRESS  (0x44A00008)
#define MTIME_RATE_HZ                ((unsigned long)100000000)
#define configTICK_CLOCK_HZ          ((unsigned long)100000000)
#define portasmHAS_CLINT              0

#include <stddef.h>
#include <interrupts.h>
#include <portmacro.h>

#define configNUM_THREAD_LOCAL_STORAGE_POINTERS  3

#define projCOVERAGE_TEST                          0

#define configQUEUE_REGISTRY_SIZE                 20
#define configUSE_PREEMPTION                       1
#define configUSE_TIME_SLICING                     0
#define configCPU_CLOCK_HZ          ((unsigned long)100000000)
#define configTICK_RATE_HZ          ((TickType_t)1000)
#define configMINIMAL_STACK_SIZE    ((unsigned short)1024)

#define configUSE_HEAP_SCHEME                            (3)
#define configTOTAL_HEAP_SIZE                            ((size_t)(0x8000))
#define configSUPPORT_DYNAMIC_ALLOCATION                 1
#define configUSE_NEWLIB_REENTRANT                       0
#define configMAX_TASK_NAME_LEN                          ( 16 )
#define configUSE_16_BIT_TICKS                           0
#define configIDLE_SHOULD_YIELD                          0
#define configUSE_CO_ROUTINES                            0
#define configMAX_PRIORITIES                             (10)
#define configMAX_CO_ROUTINE_PRIORITIES                  (2)
#define configTIMER_QUEUE_LENGTH                         20
#define configTIMER_TASK_PRIORITY                    (configMAX_PRIORITIES - 1)
#define configUSE_COUNTING_SEMAPHORES                    0
#define configSUPPORT_STATIC_ALLOCATION                  1
#define configSTREAM_BUFFER_TRIGGER_LEVEL_TEST_MARGIN    2
#define configCHECK_FOR_STACK_OVERFLOW                   2

#define configUSE_IDLE_HOOK                              0
#define configUSE_TICK_HOOK                              0
#define configUSE_DAEMON_TASK_STARTUP_HOOK               0

#define configUSE_MUTEXES                         1
#define configUSE_RECURSIVE_MUTEXES               1
#define configUSE_TIMERS                          0
#define configTIMER_TASK_STACK_DEPTH              (256)

/* DEBUG: Validate saved mepc on every context switch.
   Uncomment to catch mepc corruption before context restore.
extern void vMepcValidateHook( void );
#define traceTASK_SWITCHED_IN()  vMepcValidateHook()
*/

#define configUSE_TRACE_FACILITY                  0
#define configGENERATE_RUN_TIME_STATS             0
#define configUSE_STATS_FORMATTING_FUNCTIONS      0
int get_stats_counter(void);
void setup_stats_timer(void);

#define INCLUDE_vTaskPrioritySet                  0
#define INCLUDE_uxTaskPriorityGet                 0
#define INCLUDE_vTaskDelete                       0
#define INCLUDE_vTaskCleanUpResources             0
#define INCLUDE_vTaskSuspend                      0
#define INCLUDE_vTaskDelayUntil                   1
#define INCLUDE_vTaskDelay                        1
#define INCLUDE_uxTaskGetStackHighWaterMark       0
#define INCLUDE_uxTaskGetStackHighWaterMark2      0
#define INCLUDE_xTaskGetSchedulerState            1
#define INCLUDE_xTimerGetTimerDaemonTaskHandle    0
#define INCLUDE_xTaskGetIdleTaskHandle            0
#define INCLUDE_xTaskGetHandle                    0
#define INCLUDE_eTaskGetState                     0
#define INCLUDE_xSemaphoreGetMutexHolder          0
#define INCLUDE_xTimerPendFunctionCall            0
#define INCLUDE_xTaskAbortDelay                   0

#ifdef HEAP3
    #define xPortGetMinimumEverFreeHeapSize    ( x )
    #define xPortGetFreeHeapSize               ( x )
#endif

#endif /* FREERTOS_CONFIG_H */
