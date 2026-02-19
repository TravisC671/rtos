#include <FreeRTOSConfig.h>
#include <FreeRTOS.h>
#include <task.h>
#include <uart.h>

void stats_task(void *pvParameters);

int get_stats_counter();
void setup_stats_timer();