#include <stdlib.h>
#include <stdint.h>



// uart is driven by the system clock
#define UART_clkin 50000000


void uart_init(int baud_rate);
void uart_put_char(char c);
void uart_write_string(char *s);
char uart_get_char();
void uart_read_string(char *s, int maxLength);

