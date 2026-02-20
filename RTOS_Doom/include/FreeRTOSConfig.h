
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

//-----------------------------------------------------------
// Cortex-M specific definitions. */
//
// CMSIS is a small library, mostly implemented in a header file, that
//  gives access to some Cortex-M system registers. It has a
//  relatively simple API.  We can include it here so that all files
//  that include this one can use CMSIS.
//
// Include CMSIS for your device, or comment this out and set the
// parameters manually
#include <ARMCM3.h> 
// __NVIC_PRIO_BITS will be specified when CMSIS is being used. BUT
//  ONLY IF YOU INCLUDE THE CMSIS HEADER PREVIOUSLY
#ifdef __NVIC_PRIO_BITS
  #define configPRIO_BITS	                         __NVIC_PRIO_BITS
#else
  #define configPRIO_BITS				 3
#endif

#define configENABLE_TRUSTZONE  0

// The ARM Cortex supports nested interrupts. (Higher priority
// interrupts can interrupt lower priority interrupts). ISRs that call
// any API functions must have an interrupt priority equal to or lower
// than configMAX_SYSCALL_INTERRUPT_PRIORITY.  Higher numbers mean
// lower priority. Our CPU has 3 bits of priority in the NVIC, so the
// following 0x80 actually translates to 0xb100. ISRs at priority 4,
// 5, 6, and 7 can call FreeRTOS API functions.
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    0x80

//-----------------------------------------------------------
// START OF MAIN FREERTOS CONFIGURATION
// To learn about these (and other) FreeRTOS configuration settings,
// read the "Configuration" section of the FreeRTOS API manual. Also,
// read the FreeRTOS source code files to learn about features that
// are not included in the documentation.

#define projCOVERAGE_TEST                          0

#define configQUEUE_REGISTRY_SIZE                 20
#define configUSE_PREEMPTION                       1
#define configUSE_TIME_SLICING                     0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION    0
#define configCPU_CLOCK_HZ          ((unsigned long)50000000)
#define configTICK_RATE_HZ          ((TickType_t)1000)
#define configMINIMAL_STACK_SIZE    ((unsigned short)256)

/* HEAP SCHEME is either 
   1 (only alloc), 
   2 (alloc/free), 
   3 (malloc), 
   4 (coalesc blocks), 
   5 (multiple blocks), 
   6 (newlib) */
/* The Heap 6 implementation with using newlib needs three symbols
   defined by the linker:
    Heap Base (default __HeapBase): start address of heap
    Heap Limit (default __HeapLimit): end address of heap
    Heap size (default __heap_size): size of the heap memory in bytes */

#define configUSE_HEAP_SCHEME                            (3)

/* TOTAL_HEAP_SIZE is not used with heap scheme 6 */
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
#define configCHECK_FOR_STACK_OVERFLOW			 0

//#define configUSE_MALLOC_FAILED_HOOK                     1
//extern void malloc_failed();
//#define vApplicationMallocFailedHook(x)     malloc_failed()

#define configUSE_IDLE_HOOK                              0
#define configUSE_TICK_HOOK                              0
#define configUSE_DAEMON_TASK_STARTUP_HOOK               0


void vAssertCalled( unsigned line, const char * const filename );
//#define configASSERT_DEFINED                             1
//#define configASSERT( x )    if( ( x ) == 0 ) vAssertCalled()

/* Define to trap certain errors in application code during development. */
#define ASSERT( x ) if( ( x ) == 0 ) vAssertCalled( __LINE__, __FILE__);
// #define ASSERT( x )



#define configUSE_MUTEXES                         1
#define configUSE_RECURSIVE_MUTEXES               1
#define configUSE_TIMERS                          0
#define configTIMER_TASK_STACK_DEPTH              (256)

// Set up everything needed for statistics reporting.  All af the
// following must be set to get the runtime stats task to run
#define configUSE_TRACE_FACILITY                  1
#define configGENERATE_RUN_TIME_STATS             1
#define configUSE_STATS_FORMATTING_FUNCTIONS      1 
int get_stats_counter();
void setup_stats_timer();
#define portGET_RUN_TIME_COUNTER_VALUE          get_stats_counter
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS  setup_stats_timer 

/* Optional FreeRTOS functionality.  Set the following definitions to
   1 to include the API function, or zero to exclude the API
   function. */
#define INCLUDE_vTaskPrioritySet                  0
#define INCLUDE_uxTaskPriorityGet                 0
#define INCLUDE_vTaskDelete                       0
#define INCLUDE_vTaskCleanUpResources             0
#define INCLUDE_vTaskSuspend                      0
#define INCLUDE_vTaskDelayUntil                   1
#define INCLUDE_vTaskDelay                        1
#define INCLUDE_uxTaskGetStackHighWaterMark       0
#define INCLUDE_uxTaskGetStackHighWaterMark2      0
#define INCLUDE_xTaskGetSchedulerState            0
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
