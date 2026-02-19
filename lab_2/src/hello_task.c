#include <hello_task.h>

void hello_task(void *pvParameters)
{
  char buffer[100];

  int i = 31;
  while(1) {
    snprintf(buffer, sizeof(buffer), "\033[2J\033[%dmTravis Calhoun's Project\033[0m\n\r", i++);
    uart_write_string(buffer);
    vTaskDelay(pdMS_TO_TICKS(1000));

    if (i > 36) {
      i = 31;
    }
  }
}

