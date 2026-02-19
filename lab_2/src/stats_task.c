#include <stats_task.h>
#include <AXI_timer.h>

static int stats_timer = -1;
static int stats_count = 0;

void stats_timer_handler() {
    stats_count++;
}

int get_stats_counter() {
    return stats_count;
}


void setup_stats_timer() {
}

void stats_task(void *pvParameters)
{
    static char stats_buff[1024];
    stats_timer = AXI_TIMER_allocate();
    
    if (stats_timer < 0) {
        return;
    }
    
    AXI_TIMER_set_handler(stats_timer, stats_timer_handler);
    AXI_TIMER_set_repeating(stats_timer, AXI_TIMER_HZ_TO_COUNT(configTICK_RATE_HZ * 5));
    AXI_TIMER_enable_interrupt(stats_timer);

    AXI_TIMER_enable(stats_timer);

    while (1)
    {
        vTaskGetRunTimeStats(stats_buff);
        uart_write_string(stats_buff);
        uart_write_string("\n\r");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

