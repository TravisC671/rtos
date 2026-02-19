#include <uart.h>

int main()
{
  // configure the uart for 9600/N/8/2
  uart_init(9600);

  for(int i = 0; i<10;i++)
    uart_write_string("Hello World\n\r");

  uart_write_string("\n\r");
  
  while(1); 
}

