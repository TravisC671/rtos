#include <FreeRTOSConfig.h>
#include <stats_task.h>
#include <task.h>
#include <stdio.h>
#include <string.h>
#include <UART_16550.h>
#include <AXI_timer.h>
#include <ANSI_terminal.h>
#include <uart_driver_table.h>
#include <malloc.h>
#include <stdlib.h>

extern char __HeapStart;
extern char __HeapTop;

#define STATS_UART UART0

static int stats_counter=0;

int get_stats_counter()
{
  return stats_counter;
}

static void stats_handler()
{
  stats_counter++;
}

void setup_stats_timer()
{
  int timer;
  timer = AXI_TIMER_allocate();
  AXI_TIMER_set_handler(timer,stats_handler);
  AXI_TIMER_set_repeating(timer,AXI_TIMER_HZ_TO_COUNT(20000));
}

/* -----------------------------------------------------------
   Custom runtime stats with three columns:
     Window  - CPU% over a sliding 10-second window
     Peak    - worst-case CPU% seen in any 2-second interval
     Avg     - cumulative average CPU% since boot
   ----------------------------------------------------------- */

#define MAX_TASKS     12
#define SAMPLE_MS     2000
#define WINDOW_SLOTS  5     /* 5 x 2 s = 10 s sliding window */

/* circular buffer of cumulative runtime snapshots */
static uint32_t snap_task[WINDOW_SLOTS][MAX_TASKS];
static uint32_t snap_total[WINDOW_SLOTS];

/* first-ever snapshot for cumulative average */
static uint32_t start_task[MAX_TASKS];
static uint32_t start_total;

/* peak permille (percentage * 10) per task */
static uint32_t peak_pm[MAX_TASKS];

/* task metadata captured on first sample */
static char     tnames[MAX_TASKS][configMAX_TASK_NAME_LEN];
static int      tmap[MAX_TASKS];  /* xTaskNumber -> slot index */
static int      num_tasks;

static int      snap_idx;
static int      snap_count;

/* map a TaskStatus_t into our slot array; returns slot index */
static int get_slot(UBaseType_t task_num)
{
  int i;
  for(i = 0; i < num_tasks; i++)
    {
      if(tmap[i] == (int)task_num)
        return i;
    }
  /* not found – add it if room */
  if(num_tasks < MAX_TASKS)
    {
      tmap[num_tasks] = (int)task_num;
      num_tasks++;
      return num_tasks - 1;
    }
  return -1;
}

/* compute permille (pct * 10) from a task delta and total delta.
   returns 0 if total_delta is zero. */
static uint32_t calc_permille(uint32_t task_delta, uint32_t total_delta)
{
  if(total_delta == 0)
    return 0;
  return (task_delta * 1000) / total_delta;
}

/* format a permille value as " NN.N%" into buf (7 chars + nul) */
static void fmt_pct(char *buf, uint32_t pm)
{
  sprintf(buf, "%3lu.%lu%%", (unsigned long)(pm / 10),
          (unsigned long)(pm % 10));
}

void stats_task(void *pvParameters)
{
  static TaskStatus_t status_array[MAX_TASKS];
  char *line;
  char *mem_buffer;
  size_t line_size;
  size_t mem_size;
  static char col1[8], col2[8], col3[8];
  UBaseType_t n_tasks;
  uint32_t total_runtime;
  uint32_t cur_task[MAX_TASKS];
  uint32_t cur_total;
  int prev;
  int oldest;
  uint32_t win_task_delta, win_total_delta;
  uint32_t int_task_delta, int_total_delta;
  uint32_t avg_task_delta, avg_total_delta;
  uint32_t pm;
  struct mallinfo mi;
  size_t heap_total;
  size_t heap_used;
  uint32_t heap_pct;
  int i, slot;
  int display_row;

  snap_idx = 0;
  snap_count = 0;
  num_tasks = 0;
  memset(snap_task, 0, sizeof(snap_task));
  memset(snap_total, 0, sizeof(snap_total));
  memset(start_task, 0, sizeof(start_task));
  memset(peak_pm, 0, sizeof(peak_pm));
  memset(tnames, 0, sizeof(tnames));
  memset(tmap, 0, sizeof(tmap));

  line_size = 80 + (rand() % (80 * 99));
  mem_size = 64 + (rand() % (64 * 99));
  line = (char *)malloc(line_size);
  mem_buffer = (char *)malloc(mem_size);

  while(1)
    {
      /* collect raw data from FreeRTOS */
      n_tasks = uxTaskGetSystemState(status_array, MAX_TASKS,
                                     &total_runtime);

      /* build cur_task[] indexed by our slot numbers */
      memset(cur_task, 0, sizeof(cur_task));
      cur_total = total_runtime;
      for(i = 0; i < (int)n_tasks; i++)
        {
          slot = get_slot(status_array[i].xTaskNumber);
          if(slot >= 0)
            {
              cur_task[slot] = status_array[i].ulRunTimeCounter;
              /* copy name on first encounter */
              if(tnames[slot][0] == '\0')
                strncpy(tnames[slot], status_array[i].pcTaskName,
                        configMAX_TASK_NAME_LEN - 1);
            }
        }

      /* store snapshot */
      for(i = 0; i < num_tasks; i++)
        snap_task[snap_idx][i] = cur_task[i];
      snap_total[snap_idx] = cur_total;

      /* on first sample, record start values */
      if(snap_count == 0)
        {
          for(i = 0; i < num_tasks; i++)
            start_task[i] = cur_task[i];
          start_total = cur_total;
        }

      snap_count++;

      /* only display once we have at least 2 samples */
      if(snap_count >= 2)
        {
          /* previous sample index (for peak tracking) */
          prev = (snap_idx + WINDOW_SLOTS - 1) % WINDOW_SLOTS;

          /* oldest sample for the 10 s window */
          if(snap_count > WINDOW_SLOTS)
            oldest = (snap_idx + 1) % WINDOW_SLOTS;
          else
            oldest = 0; /* use first sample if window not full */

          /* update peak and build display */
          ANSI_uart.tx_lock(STATS_UART, portMAX_DELAY);

          /* heap info: use mallinfo directly since xPortGetFreeHeapSize
             has a bug (heapBytesRemaining is unsigned, init check fails) */
          mi = mallinfo();
          heap_total = (size_t)(&__HeapTop - &__HeapStart);
          heap_used = (size_t)mi.uordblks;
          heap_pct = 0;
          if(heap_total > 0)
            heap_pct = (uint32_t)((heap_used * 100) / heap_total);
          sprintf(mem_buffer, "Heap Used: %u / %u (%lu%%)",
                  (unsigned)heap_used, (unsigned)heap_total,
                  (unsigned long)heap_pct);
          ANSI_moveTo(STATS_UART, 6, 0);
          ANSI_uart.write_string(STATS_UART, mem_buffer, portMAX_DELAY);
          ANSI_cleartoeol(STATS_UART);

          /* header */
          ANSI_moveTo(STATS_UART, 8, 0);
          sprintf(line, "%-16s %6s %6s %6s",
                  "Task", "Window", "Peak", "Avg");
          ANSI_uart.write_string(STATS_UART, line, portMAX_DELAY);

          /* divider */
          ANSI_moveTo(STATS_UART, 9, 0);
          ANSI_uart.write_string(STATS_UART,
              "---------------- ------ ------ ------", portMAX_DELAY);

          /* per-task rows: iterate status_array so each live task
             appears exactly once */
          display_row = 10;
          for(i = 0; i < (int)n_tasks; i++)
            {
              slot = get_slot(status_array[i].xTaskNumber);
              if(slot < 0)
                  continue;

              /* interval delta (for peak) */
              int_task_delta = cur_task[slot] - snap_task[prev][slot];
              int_total_delta = cur_total - snap_total[prev];
              pm = calc_permille(int_task_delta, int_total_delta);
              if(pm > peak_pm[slot])
                peak_pm[slot] = pm;

              /* window delta */
              win_task_delta = cur_task[slot] - snap_task[oldest][slot];
              win_total_delta = cur_total - snap_total[oldest];

              /* average delta */
              avg_task_delta = cur_task[slot] - start_task[slot];
              avg_total_delta = cur_total - start_total;

              fmt_pct(col1, calc_permille(win_task_delta, win_total_delta));
              fmt_pct(col2, peak_pm[slot]);
              fmt_pct(col3, calc_permille(avg_task_delta, avg_total_delta));

              sprintf(line, "%-16s %6s %6s %6s",
                      status_array[i].pcTaskName, col1, col2, col3);

              ANSI_moveTo(STATS_UART, display_row, 0);
              ANSI_uart.write_string(STATS_UART, line, portMAX_DELAY);
              display_row++;
            }

          ANSI_uart.tx_unlock(STATS_UART);
        }

      /* advance circular buffer index */
      snap_idx = (snap_idx + 1) % WINDOW_SLOTS;

      vTaskDelay(pdMS_TO_TICKS(SAMPLE_MS));
    }
}

/* Structure that will hold the TCB of the task being created. */
StaticTask_t stats_TCB;

/* Buffer that the task being created will use as its stack. Note this
is an array of StackType_t variables. The size of StackType_t is
dependent on the RTOS port. */
StackType_t stats_stack[ STATS_STACK_SIZE ];
