// This file implements the API for the 16550 UART driver.

// comment or delete the next line when you start part 4 of the lab
// #define ORIGINAL_PUT_CHAR

#define UART_16550_USE_STATIC_ALLOCATION
#define UART_16550_RX_BUFFER_SIZE 128
#define UART_16550_TX_BUFFER_SIZE 512

// By including our header, we ensure that the header and the C
// file agree about the function definitions.

#include <UART_16550.h>
#include <device_addrs.h>
#include <semphr.h>
#include <stream_buffer.h>

// -----------------------------------------------------------------------
// No other code needs to see the internals of this UART driver, so we
// hide all the data definitions in this file. The C standard does not
// specify the order of bits in a bit field, so this is technically
// not fully portable.  However, most compilers will give us what we
// want. If not, we can change the order or come up with a different
// way of specifying the bits.

// Define the layout for the IER
typedef struct
{
  volatile unsigned ERBFI : 1;
  volatile unsigned ETBEI : 1;
  volatile unsigned ELSI : 1;
  volatile unsigned EDSSI : 1;
  volatile unsigned unused : 28;
} IER_t;

// Define the layout for the IIR
typedef struct
{
  volatile unsigned INTPEND : 1;
  volatile unsigned INTID2 : 3;
  volatile unsigned reserved : 2;
  volatile unsigned FIFOEN : 2;
  volatile unsigned unused : 24;
} IIR_t;

// Define the layout for the FCR
typedef struct
{
  volatile unsigned FIFOEN : 1;
  volatile unsigned RF_reset : 1;
  volatile unsigned XF_reset : 1;
  volatile unsigned DMA_mode : 1;
  volatile unsigned reserved : 2;
  volatile unsigned RFTL : 2;
  volatile unsigned unused : 24;
} FCR_t;

// Define the layout for the LCR
typedef struct
{
  volatile unsigned WLS : 2;
  volatile unsigned STB : 1;
  volatile unsigned PEN : 1;
  volatile unsigned EPS : 1;
  volatile unsigned SP : 1;
  volatile unsigned BREAK : 1;
  volatile unsigned DLAB : 1; // this bit must be 1 to set the baud rate.
  volatile unsigned unused : 24;
} LCR_t;

// Define the layout for the MCR
typedef struct
{
  volatile unsigned DTR : 1;
  volatile unsigned RTS : 1;
  volatile unsigned Out1 : 1;
  volatile unsigned Out2 : 1;
  volatile unsigned Loop : 1;
  volatile unsigned unused : 27;
} MCR_t;

// Define the layout for the LSR
typedef struct
{
  volatile unsigned DR : 1;
  volatile unsigned OE : 1;
  volatile unsigned PE : 1;
  volatile unsigned FE : 1;
  volatile unsigned BI : 1;
  volatile unsigned THRE : 1;
  volatile unsigned TEMT : 1;
  volatile unsigned RFE : 1;
  volatile unsigned unused : 24;
} LSR_t;

// Define the layout for the MSR
typedef struct
{
  volatile unsigned DCTS : 1;
  volatile unsigned DDSR : 1;
  volatile unsigned TERI : 1;
  volatile unsigned DDCD : 1;
  volatile unsigned CTS : 1;
  volatile unsigned DSR : 1;
  volatile unsigned RI : 1;
  volatile unsigned DCD : 1;
  volatile unsigned unused : 24;
} MSR_t;

// -----------------------------------------------------------------------
// Define the register layout of the AXI UART device

typedef volatile struct
{
  union
  {
    volatile uint32_t RBR; // used when reading data recieved(LCR(7) == 0)
    volatile uint32_t THR; // used when writig data to transmit (LCR(7) == 0)
    volatile uint32_t DLL; // used when setting the BAUD rate (LCR(7) == 1)
  };
  union
  {
    volatile IER_t IER;    // used to set or read interrupt config (LCR(7) == 0)
    volatile uint32_t DLH; // used when setting BAUD rate (LCR(7) == 1)
  };
  union
  {
    volatile IIR_t IIR; // used to identfy cause of an interrupt (read)
    volatile FCR_t FCR; // used to set up the FIFO (write)
  };
  volatile LCR_t LCR;    // Line control register
  volatile MCR_t MCR;    // Modem control register
  volatile LSR_t LSR;    // Line status register
  volatile MSR_t MSR;    // Modem status register
  volatile uint32_t SCR; // Scratch register
} UART_16550_t;

// END of register definitions
// -----------------------------------------------------------------------

// The transmitter code for each UART is implemented as a software
// state machine.  These are the possible states.
typedef enum
{
  TX_EMPTY,
  TX_FIFO,
  TX_BUFFER
} UART_tx_state_t;

// Define a struct that holds all of the private information about a
// single UART.
typedef struct
{
  UART_16550_t *dev;              // Base address of the UART device
  unsigned interrupt_number;      // NVIC IRQ number for this UART
  StreamBufferHandle_t RX_buffer; // stream buffer for received data
  StreamBufferHandle_t TX_buffer; // stream buffer for data to be transmitted
  SemaphoreHandle_t RX_mutex;     // Recursive mutex for the receiver
  SemaphoreHandle_t TX_mutex;     // Recursive mutex for the transmitter
  UART_tx_state_t tx_state;       // Transmitter state for this UART
} UART_16550_descriptor_t;

// Define an array that holds the private information for each
// UART. The base and interrupt numbers are defined in device_addrs.h.
static UART_16550_descriptor_t uart[] = {
    {UART0_base, UART0_IRQ, NULL, NULL, NULL, NULL, TX_EMPTY},
    {UART1_base, UART1_IRQ, NULL, NULL, NULL, NULL, TX_EMPTY}};

// Get the compiler to compute the number of UARTS that are in the
// abouve array.
#define NUM_UARTS (sizeof(uart) / sizeof(UART_16550_descriptor_t))

// END of UART definitions and descriptor definitions.
// -----------------------------------------------------------------------

//             BEGINNING OF CODE

/*****************************************************************************/
// This function is the ISR for transmitter interrupts.
static void handle_tx_interrupt(UART_16550_descriptor_t *device,
                                BaseType_t *HigherPriorityTaskWoken)
{
  char buf[16];
  int bytes_received;
  // We got an interrupt indicating that the UART FIFO just became
  // empty.  We must decide what to do based on the current state of
  // the transmitter software state machine.
  switch (device->tx_state)
  {

  case TX_BUFFER:
    // If the software state machine is in the TX_BUFFER state, then
    // You can move up to 16 bytes from the transmit stream buffer
    // to the transmit FIFO.  Move as many bytes as you can.
    bytes_received = xStreamBufferReceiveFromISR(device->TX_buffer,
                                                 buf,
                                                 16,
                                                 HigherPriorityTaskWoken);
    for (int i = 0; i < bytes_received; i++)
      device->dev->THR = buf[i];
    // If the stream buffer is empty, change the state of the
    // transmitter software state machine.
    if (xStreamBufferIsEmpty(device->TX_buffer) == pdTRUE)
    {
      //   If you moved some bytes to the FIFO, then the new state is
      //   TX_FIFO.
      if (bytes_received)
        device->tx_state = TX_FIFO;
      //   Otherwise, the new state is TX_EMPTY. Optionally, disable
      //   the transmitter interrupt.
      else
      {
        device->tx_state = TX_EMPTY;
        device->dev->IER.ETBEI = 0; // disable Xmit interrupt
      }
    }
    break;

  case TX_FIFO:
    // If the software state machine is in the TX_FIFO state then we
    // know that the FIFO just became empty and there is nothing in
    // the stream buffer (If there was something in the buffer, the
    // state would be TX_BUFFER). We can change the state to
    // TX_EMPTY and optionally disable the transmit interrupt.
    device->tx_state = TX_EMPTY;
    device->dev->IER.ETBEI = 0; // disable transmit interrupt
    break;

  case TX_EMPTY:
    // If the state is TX_EMPTY, then we have nothing to do.  This
    // should never happen, so hang in an infinite loop for
    // debugging.
    while (1)
      ;
    device->dev->IER.ETBEI = 0; // disable transmit interrupt
    break;

  default:
    // Somehow the ISR got called in an invalid tx_state. This
    // should never happen, so hang in an infinite loop for
    // debugging.
    while (1)
      ;
  }
}

/*****************************************************************************/
// This is the ISR for all 16550 UARTS on the system it is given a
// UART descripctor struct that describes the UART.
static BaseType_t UART_handler(UART_16550_descriptor_t *device)
{
  IIR_t iir;
  uint8_t ch;
  BaseType_t HigherPriorityTaskWoken = 0;
  BaseType_t result;

  // This device could have more than one interrupt active. It will
  // prioritize them and we can handle them one at a time.

  // Read the IIR register and find out what interrupt type is being
  // signalled.  Every time we read the IIR, it changes.  Therefore,
  // we cannot read it multiple times to check bits individually.  We
  // need to read the register into a local IIR_t variable, and then
  // check the bit fields in that.
  iir = device->dev->IIR; // read all of the IIR bits into local variable.

  // Repeat while this device has more interrupts to service
  while (!iir.INTPEND) // use local variable to check INTPEND. INTPEND
                       // is active low!
  {
    switch (iir.INTID2) // use local variable to check INTID2
    {
    case 0b010: // Received Data Available
    case 0b110: // Character Timeout
                // Move as many characters as possible from the UART FIFO to
                // the RX stream buffer
      while (device->dev->LSR.DR)
      {
        ch = device->dev->RBR;
        result = xStreamBufferSendFromISR(device->RX_buffer,
                                          &ch,
                                          1,
                                          &HigherPriorityTaskWoken);
      }
      break;

    case 0b001: // Transmitter Holding Register Empty
      // Call a function to handle the transmitter interrupt.
      // This makes the code a little easier to read and manage.
      handle_tx_interrupt(device, &HigherPriorityTaskWoken);
      break;

    default:
      // We got an interrupt from a source that should not be enabled.
      while (1)
        ;
    }

    // Re-read all of the bits of the IIR into our local variable.
    iir = device->dev->IIR;
  }

  // The interrupts should now be clear in the device, and now we must
  // clear the interrupt in the NVIC.
  NVIC_ClearPendingIRQ(device->interrupt_number);

  return HigherPriorityTaskWoken;
}

/*****************************************************************************/
// This is the ISR for UART0. Put this function in the interrupt
// vector table.
void UART0_handler()
{ // pass pointer to uart0 descriptor to the real handler function
  BaseType_t hptw = UART_handler(uart);
  // If reading or writing a stream buffer has unblockd a task with
  // higher priority than the one currently running, then run the
  // scheduler.
  portYIELD_FROM_ISR(hptw);
}

/*****************************************************************************/
// This is the ISR for UART1. Put this function in the interrupt
// vector table.
void UART1_handler()
{ // pass pointer to uart1 descriptor to the real handler function
  BaseType_t hptw = UART_handler(uart + 1);
  // If reading or writing a stream buffer has unblockd a task with
  // higher priority than the one currently running, then run the
  // scheduler.
  portYIELD_FROM_ISR(hptw);
}

/*****************************************************************************/
// Initialize the 16550 UART driver and all 16550 UART devices. This
// should be called once during the OS initialisation phase of
// bootup/reset.
void UART_16550_init()
{
  // Create the stream buffers and mutexes.
#ifdef UART_16550_USE_STATIC_ALLOCATION
  // If you want to use static allocation, then declare the buffer
  // storage, buffer structs. They are static, so the compiler will
  // put them in the .data or .bss section.
  static uint8_t RX_buffer_data[NUM_UARTS][UART_16550_RX_BUFFER_SIZE];
  static uint8_t TX_buffer_data[NUM_UARTS][UART_16550_TX_BUFFER_SIZE];
  static StaticStreamBuffer_t RX_buffer[NUM_UARTS];
  static StaticStreamBuffer_t TX_buffer[NUM_UARTS];
  static StaticSemaphore_t RX_mutex[NUM_UARTS];
  static StaticSemaphore_t TX_mutex[NUM_UARTS];
  // Create the stream buffers and mutexes using static allocation.
  for (int i = 0; i < NUM_UARTS; i++)
  {
    uart[i].RX_buffer =
        xStreamBufferCreateStatic(UART_16550_RX_BUFFER_SIZE, 1,
                                  RX_buffer_data[i], &RX_buffer[i]);
    uart[i].TX_buffer =
        xStreamBufferCreateStatic(UART_16550_TX_BUFFER_SIZE, 1,
                                  TX_buffer_data[i], &TX_buffer[i]);
    uart[i].RX_mutex =
        xSemaphoreCreateRecursiveMutexStatic(&RX_mutex[i]);
    uart[i].TX_mutex =
        xSemaphoreCreateRecursiveMutexStatic(&TX_mutex[i]);
  }
#else
  // Create the stream buffers and mutexes using dynamic
  // allocation. They will be stored in the heap.
  for (int i = 0; i < NUM_UARTS; i++)
  {
    uart[i].RX_buffer = xStreamBufferCreate(UART_16550_RX_BUFFER_SIZE, 1);
    uart[i].TX_buffer = xStreamBufferCreate(UART_16550_TX_BUFFER_SIZE, 1);
    uart[i].RX_mutex = xSemaphoreCreateRecursiveMutex();
    uart[i].TX_mutex = xSemaphoreCreateRecursiveMutex();
  }
#endif

  // In some cases, we may want to mMake sure that all of the bits in
  // all of the UARTS are set to their reset values. If so, finish the
  // following code:
  // for( int i = 0; i < NUM_UARTS; i++)
  //   {
  //     uart[i].dev.LCR = (LCR_t)0; // set all bits in the LCR to zero
  // ...

  // The UART driver and devices are initialized.
}

/*****************************************************************************/
/* Set the baud, rate, parity, bits per frame, and number of stop bits
 * for the given UART, and enable the appropriate interrupt(s).
 *
 * - baud can be anything that the hardware can support.
 * - parity should be UART_16550_PARITY_EVEN, UART_16550_PARITY_ODD, or
 *     UART_16550_PARITY_NONE
 * - bits can be between 5 and 8
 * - stop_bits can be 1 or 2
 *
 * In most RTOS use cases, this should be called only once for each
 * UART, in the application initialization phase of bootup/reset.  If
 * only a single task will use the UART, then this can be called in the
 * startup code of that task, before it enters its main loop.
 */
void UART_16550_configure(int UART, int baud, int parity, int bits, int stop_bits)
{
  // Assert that the uart number is good.
  ASSERT(UART >= 0 && UART < NUM_UARTS);

  // Calculate the baud rate divisor
  unsigned divisor = UART_16550_clk / (baud << 4);
  // Extremely high baud rates have too much error and just won't work.
  ASSERT(divisor > 24);
  // Make sure divisor fits in 16 bits
  ASSERT(divisor < 1 << 16);

  // Write the baud rate divisor
  LCR_t lcr;
  lcr.DLAB = 1; // prepare to write baud rate divisor
  uart[UART].dev->LCR = lcr;
  uart[UART].dev->DLL = divisor & 0xFF; // write low byte
  uart[UART].dev->DLH = divisor >> 8;   // write high byte
  lcr.DLAB = 0;                         // finish setting baud rate on next write to LCR

  // Set the parity (make sure that it is one of the three valid options)
  ASSERT(parity >= 0 && parity < 3);
  if (parity == UART_PARITY_NONE)
    lcr.PEN = 0; // No parity bit
  else
  {
    if (parity == UART_PARITY_EVEN)
      lcr.EPS = 1; // Set even
    else
      lcr.EPS = 0; // Set odd
    lcr.PEN = 1;   // Enable parity bit
  }

  // Set the number of data bits
  ASSERT(bits > 4 && bits < 9);
  lcr.WLS = bits - 5; // set the word length

  // Set the number of stop bits
  ASSERT(stop_bits > 0 && stop_bits < 3);
  lcr.STB = stop_bits - 1; // number of stop bits
  uart[UART].dev->LCR = lcr;

  // Reset and enable the FIFOs.  You cannot read from the FCR, so we
  // set up a local variable and write it all at once.
  FCR_t fcr = {0};
  // Reset and enable the FIFOs.
  fcr.RF_reset = 1;
  fcr.XF_reset = 1;
  fcr.FIFOEN = 1;
  uart[UART].dev->FCR = fcr;

  // Enable receiver and transmitter interrupts. Disable line control
  // and modem status interrupts.  You cannot read from the IER, so we
  // set up a local variable and write it all at once.
  IER_t ier = {0};
  ier.ERBFI = 1;             // enable receiver interrupt
  ier.ETBEI = 0;             // enable transmitter interrupt
  ier.ELSI = 0;              // disable line control interrupt
  ier.EDSSI = 0;             // disable modem status interrupt
  uart[UART].dev->IER = ier; // disable modem status interrupt

  // Enable interrupts on the NVIC
  NVIC_EnableIRQ(uart[UART].interrupt_number);
}

/*****************************************************************************/
/* Acquire the given UART transmitter mutex so that no other task can
   write to it. Returns pdPASS if the lock is acquired. */
BaseType_t UART_16550_tx_lock(int UART,
                              TickType_t xTicksToWait)
{
  // Assert that the uart number is good.
  ASSERT(UART >= 0 && UART < NUM_UARTS);
  return xSemaphoreTakeRecursive(uart[UART].TX_mutex, xTicksToWait);
}

/*****************************************************************************/
/* Unlock the given UART transmitter so that other tasks can write to
   it. */
void UART_16550_tx_unlock(int UART)
{
  // Assert that the uart number is good.
  ASSERT(UART >= 0 && UART < NUM_UARTS);
  xSemaphoreGiveRecursive(uart[UART].TX_mutex);
}

/*****************************************************************************/
/* Try to write a character to the UART */
#ifdef ORIGINAL_PUT_CHAR
// origial non-interrupt-driven version of put char.  Use this for
// parts 1, 2 and 3 of the lab.
BaseType_t UART_16550_put_char(int UART,
                               char c,
                               TickType_t xTicksToWait)
{
  // This is the original code.  There is a #define at the top of this
  // file that selects this implementation.  Comment or delete that
  // #define to use the interrupt-driven implementation.  For part
  // three of the lab, add mutexes to this version.

  // Acquire the transmitter mutex for this UART, so that other threads
  // cannot interfere ( the ISR can sill interrupt us).

  // ------------ Insert code here

  // Wait until transmitter holding register is empty
  while (!uart[UART].dev->LSR.THRE)
    ;
  // Send the character
  uart[UART].dev->THR = c;

  // Release the mutex.

  // ------------ Insert code here

  return pdPASS;
}

#else

// Interrupt-diven version of put_char. Use this for parts 4 and 5 of
// the lab.
BaseType_t UART_16550_put_char(int UART,
                               char c,
                               TickType_t xTicksToWait)
{

  // Assert that the uart number is good.
  ASSERT(UART >= 0 && UART < NUM_UARTS);

  // We must call VPortExitCritical() before exiting this function,
  // but we MAY call it early.  We must only call it ONCE. So lets
  // create a local flag to keep track of when we are in the critical
  // section.
  int still_in_critical_section = 1;
  BaseType_t result = pdPASS;
  UART_16550_descriptor_t *my_uart = uart + UART;

  // Acquire the transmitter mutex for this UART, so that other threads
  // cannot interfere ( the ISR can sill interrupt us).
  result = xSemaphoreTakeRecursive(my_uart->TX_mutex, portMAX_DELAY);
  if (result == pdPASS)
  {
    // Enter a CRITICAL SECTION, so that even the ISR cannot interrupt us
    vPortEnterCritical();

    // Make decisions based on the current state of the transmit
    // software state machine.
    switch (uart[UART].tx_state)
    {
    case TX_EMPTY:
      // If the software state machine is in the TX_EMPTY state, then
      // write our character directly to the UART FIFO, and change the
      // transmit software state machine state to TX_FIFO to indicate
      // that there is data in the UART FIFO, but the transmit stream
      // buffer is empty.
      my_uart->dev->THR = c;
      my_uart->dev->IER.ETBEI = 1; // enable transmit interrupt
      my_uart->tx_state = TX_FIFO;
      break;
    case TX_FIFO:
      // If the software state machine is in the TX_FIFO state, then
      // write the character to the transmit stream buffer and change
      // the software state machine state to TX_BUFFER to indicated
      // that there is data in the transmit stream buffer.

      // The next statement should never block because we know that
      // the stream buffer is empty. (We are in TX_FIFO state).
      xStreamBufferSend(my_uart->TX_buffer, &c, 1, portMAX_DELAY);
      my_uart->tx_state = TX_BUFFER;
      my_uart->dev->IER.ETBEI = 1; // enable transmit interrupt
      break;
    case TX_BUFFER:
      // If the state is TX_BUFFER, then find out how much space is
      // available in the transmit stream buffer.
      if (xStreamBufferSpacesAvailable(my_uart->TX_buffer))
        //   If the buffer is not full then we can write our
        //   character to it and continue.
        xStreamBufferSend(my_uart->TX_buffer, &c, 1, portMAX_DELAY);
      else
      {
        // Otherwise, things get a bit trickier.  The stream buffer
        // is full, so a write to the stream buffer could block this
        // task (depending on the value of xTicksToWait).  We are in
        // a critical section, so if this task blocks, then no
        // interrupts will get processed. If no interrupts are
        // processed, then there is no way that the stream buffer
        // can be read. If the stream buffer is never read, then
        // this task will never be unblocked.  We MUST exit the
        // critical section NOW, and then attempt to write to the
        // stream buffer.  If it blocks, only this thread
        // blocks. The system continues to get interrupts and
        // continues to run whatever tasks are runnable.  The ISR
        // will eventually read from the stream buffer and unblock
        // this task so that it can unlock the mutex and let other
        // tasks write to the UART.

        // Change the still_in_critical_section variable to 0, to
        // indicate that we left the critical section early.
        still_in_critical_section = 0;

        // Exit the critical section so the ISR can eventually move
        // data out of the buffer and unblock this thread.
        vPortExitCritical();

        // Write our character to the stream buffer, using the
        // timeout value that was passed in to this function, and
        // return the result of that write at the end of this
        // function.
        result = xStreamBufferSend(my_uart->TX_buffer, &c, 1, xTicksToWait);
      }
      break;
    default:
      while (1)
        ; // Illegal tx_state.  Go into infinite loop for
      // debugging.
      break;
    }

    // If we are still in the critical section, exit the critical
    // section.
    if (still_in_critical_section)
      vPortExitCritical();

    // Release the mutex.
    xSemaphoreGiveRecursive(my_uart->TX_mutex);
  }
  // Return pdPASS or pdFAIL.
  return result;
}

#endif

/*****************************************************************************/
/* Write a string to the UART. */
BaseType_t UART_16550_write_string(int UART,
                                   char *s,
                                   TickType_t xTicksToWait)
{
  // Assert that the uart number is good.
  ASSERT(UART >= 0 && UART < NUM_UARTS);
  BaseType_t result;
  // Get the TX mutex using xTicksToWait (return pdFAIL if we don't
  // get it)
  result = xSemaphoreTakeRecursive(uart[UART].TX_mutex, xTicksToWait);
  if (result == pdPASS)
  {
    // Use the put char function to send characters.  This could be
    // greatly improved.
    if (s != NULL)
      while (*s != 0 && result == pdPASS)
        result = UART_16550_put_char(UART, *(s++), xTicksToWait);
  }
  // release the TX mutex
  xSemaphoreGiveRecursive(uart[UART].TX_mutex);
  return result;
}

/*****************************************************************************/
/* Lock the given UART receiver, so that no other task can read
   from it  Returns pdPASS if the lock is acquired. */
BaseType_t UART_16550_rx_lock(int UART,
                              TickType_t xTicksToWait)
{
  // Assert that the uart number is good.
  ASSERT(UART >= 0 && UART < NUM_UARTS);
  return xSemaphoreTakeRecursive(uart[UART].RX_mutex, xTicksToWait);
}

/*****************************************************************************/
/* Unlock the given UART receiver so that other tasks can read from
   it. */
void UART_16550_rx_unlock(int UART)
{
  // Assert that the uart number is good.
  ASSERT(UART >= 0 && UART < NUM_UARTS);
  xSemaphoreGiveRecursive(uart[UART].RX_mutex);
}

/*****************************************************************************/
/* Try to read a character from the UART */
BaseType_t UART_16550_get_char(int UART, char *ch,
                               TickType_t xTicksToWait)
{
  // Assert that the uart number is good.
  ASSERT(UART >= 0 && UART < NUM_UARTS);
  BaseType_t result;
  // Get the RX mutex using xTicksToWait (return pdFAIL if we don't
  // get it)
  result = xSemaphoreTakeRecursive(uart[UART].RX_mutex, xTicksToWait);
  if (result == pdPASS)
  {
    // Attempt to read a character from the receive (RX) stream buffer
    // using xTicksToWait. It could fail (time out), so keep the value
    // returned in a local variable.
    result = xStreamBufferReceive(uart[UART].RX_buffer, ch, 1, xTicksToWait);
    // Release the mutex.
    xSemaphoreGiveRecursive(uart[UART].RX_mutex);
  }
  // Return the value we got from the attempt to read.
  return result;
}

/*****************************************************************************/
/* Try to read a string from the UART */
BaseType_t UART_16550_read_string(int UART,
                                  char *s,
                                  int maxLength,
                                  TickType_t xTicksToWait)
{
  // Assert that the uart number is good.
  ASSERT(UART >= 0 && UART < NUM_UARTS);
  BaseType_t result;
  char c;
  int length = 0;
  // Get the RX mutex using xTicksToWait (return pdFAIL if we don't
  // get it)
  result = xSemaphoreTakeRecursive(uart[UART].RX_mutex, xTicksToWait);
  if (result == pdPASS)
  {
    do
    {
      result = UART_16550_get_char(UART, &c, xTicksToWait);
      if (result == pdPASS)
        *(s++) = (c == '\n') || (c == '\r') ? 0 : c;
      length++;
    } while (c != '\n' && c != '\r' && length < maxLength && result == pdPASS);
    // make sure it is null terminated
    if (length == maxLength)
      s[length - 1] = 0;
    // release the mutex
    xSemaphoreGiveRecursive(uart[UART].RX_mutex);
  }
  return result;
}

/*****************************************************************************/
// Return the number of characters available
int UART_16550_chars_available(int UART_number)
{
  int avail = xStreamBufferBytesAvailable(uart[UART_number].RX_buffer);
  return avail;
}

/*****************************************************************************/
// Flush the UART receiver
void UART_16550_flush_rx(int UART_number)
{
  xStreamBufferReset(uart[UART_number].RX_buffer);
}
