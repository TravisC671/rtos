
#include <FreeRTOSConfig.h>
#include <FreeRTOS.h>
#include <device_addrs.h>
#include <task.h>
#include <semphr.h>
#include <LDP-001_PM_driver.h>


// Our system has a 150MHz clock for the PM devices
#define PM_CLK_FREQ 150000000

// Define the layout of the bits in the CSR
// #define OLD_PM

#ifdef OLD_PM
typedef struct{
  volatile unsigned OE:1;      // Output enable
  volatile unsigned IO:1;      // Invert output
  volatile unsigned SLFM:1;    // Synchronous load/FIFO mode
  volatile unsigned FF:1;      // FIFO full
  volatile unsigned FE:1;      // FIFO empty
  volatile unsigned RF:1;      // Reset FIFO
  volatile unsigned IA:1;      // Interrupt active
  volatile unsigned IE:1;      // Interrupt enable
  volatile unsigned PDMM:1;    // PDM mode
  volatile unsigned UNUSED:18;
  volatile unsigned FIL:5;     // FIFO interrupt level
}CSR_t;
#else
typedef struct{
  volatile unsigned OE:1;        // Output enable
  volatile unsigned IO:1;        // Invert output
  volatile unsigned SLFM:1;      // Synchronous load/FIFO mode
  volatile unsigned PDMM:1;      // PDM mode
  volatile unsigned IE:1;        // Interrupt enable
  volatile unsigned RF:1;        // Reset FIFO
  volatile unsigned UNUSED_1:2;  // two bits unused
  volatile unsigned FF:1;        // FIFO full
  volatile unsigned FE:1;        // FIFO empty
  volatile unsigned IA:1;        // Interrupt active
  volatile unsigned UNUSED_2:16; // 16 bits unused
  volatile unsigned FIL:5;       // FIFO interrupt level
}CSR_t;
#endif

// ------------------------------------------------------------
// Define the register layout for each channel of the PM device
typedef volatile struct{
  volatile CSR_t CSR;     // Control register
  // uint16_t UNUSED1;       // Unused bits
  volatile uint32_t CDR;  // Clock divisor register
  // uint16_t UNUSED2;       // Unused bits
  volatile uint32_t BCR;  // Base cycle time register
  //  uint16_t UNUSED3;       // Unused bits
  volatile uint32_t DCR ; // Duty cycle register
}channel_t;

// ------------------------------------------------------------
// Define the descriptor for each channel
typedef struct{
    volatile channel_t *dev; // address of the beginning of the
                              // channel (the CSR)
    TaskHandle_t owner;       // handle of the task that owns this
                              // channel
    void (*handler)(BaseType_t *HPTW); // address of interrupt handler for this
                                       // channel
    char mode_set;            // boolean: PDM or PWM mode has been selected
    char ct_set;              // boolean: cycle time has been set
    char fifo_set;            // boolean: fifo mode has been set
}channel_descriptor_t;

// ------------------------------------------------------------
// Create a descriptor for each channel;
static channel_descriptor_t channels[] = {
  {PMaudio_ctrl, NULL, NULL,0,0,0},
  {PM1_ctrl, NULL, NULL,0,0,0},
  {PM2_ctrl, NULL, NULL,0,0,0},
  {PM3_ctrl, NULL, NULL,0,0,0},
  {PM4_ctrl, NULL, NULL,0,0,0}
};

// Get the compiler to compute the number of channels that are
//  in the above array.
#define NUM_CHANNELS (sizeof(channels)/sizeof(channel_descriptor_t))

SemaphoreHandle_t mutex_handle;

/* Reset all PM channels' hardware to a safe state.
   No FreeRTOS dependencies — safe to call before the scheduler or
   global interrupts are enabled.  Essential for GDB reload: the CPU
   restarts (BSS zeroed, driver state cleared) but the PM hardware
   retains its previous configuration.  Without this, stale interrupts
   fire with NULL handlers causing an infinite interrupt loop. */
void PM_reset_hardware()
{
  int i;
  volatile uint32_t *csr;
  for(i = 0; i < (int)NUM_CHANNELS; i++)
    {
      /* Write the entire CSR as a single word to avoid
         read-modify-write issues with status bits (IA, FF, FE).
         Bit 5 = RF (reset FIFO).  Everything else zero
         (OE=0, IE=0, SLFM=0, PDMM=0). */
      csr = (volatile uint32_t *)&channels[i].dev->CSR;
      *csr = 0x20;   /* assert RF, disable everything */
      *csr = 0x00;   /* release RF so FIFO can be used later */
    }
}

void PM_init()
{
  static StaticSemaphore_t mutex;
  mutex_handle = xSemaphoreCreateMutexStatic(&mutex);
  // Enable interrupt in the interrupt controller.
  INTC_EnableIRQ(PM_IRQ);
}

void PM_handler() 
{
  int i;
  BaseType_t HPTW = 0;
  for(i=0;i<NUM_CHANNELS;i++)
    if(channels[i].dev->CSR.IA && channels[i].handler != NULL)
      channels[i].handler(&HPTW);
  // Clear interrupt in the interrupt controller.
  INTC_ClearPendingIRQ(PM_IRQ); 
  portYIELD_FROM_ISR(HPTW);
}

// Get exclusive access to the pulse modulator channel. 
BaseType_t PM_acquire(int channel)
{
  TaskHandle_t myhandle;
  int rval;
  ASSERT(channel >= 0 && channel < NUM_CHANNELS);
  xSemaphoreTake(mutex_handle,portMAX_DELAY);
  if(channels[channel].owner)
    rval = 0;
  else
    channels[channel].owner = xTaskGetCurrentTaskHandle();
  rval = 1;
  xSemaphoreGive(mutex_handle);
  return rval;
}

// Release the channel. This function also disables the channel.
void PM_release(int channel)
{
  ASSERT(channel >= 0 && channel < NUM_CHANNELS);
  // if current task does not own the channel, lock up for debugging
  xSemaphoreTake(mutex_handle,portMAX_DELAY);
  if(channels[channel].owner != xTaskGetCurrentTaskHandle())
    while(1);
  PM_disable(channel);
  PM_disable_FIFO(channel);
  PM_set_PWM_mode(channel);
  channels[channel].mode_set = 0;
  channels[channel].ct_set = 0;
  channels[channel].fifo_set = 0;
  channels[channel].owner = NULL;
  channels[channel].handler = NULL;
  xSemaphoreGive(mutex_handle);
}

// Set the interrupt handle function for the channel.
void PM_set_handler(int channel, void (*handler)(BaseType_t *HPTW))
{
  ASSERT(channel >= 0 && channel < NUM_CHANNELS);
  // if current task does not own the channel, lock up for debugging
  //if(channels[channel].owner != xTaskGetCurrentTaskHandle())
  //while(1);
  channels[channel].handler = handler;
  channels[channel].dev->CSR.OE = 1;
  channels[channel].dev->CSR.IE = 1;
}

// Set the base frequency and number of divisions for the
// channel.
void PM_set_cycle_time(int channel, int divisions, int base_frequency)
{
  ASSERT(channel >= 0 && channel < NUM_CHANNELS);
  // if current task does not own the channel, lock up for debugging
  //if(channels[channel].owner != xTaskGetCurrentTaskHandle())
  //while(1);
  // if output is enabled, we cannot set the BCR or CDR. Lock up for
  // debugging
  if(channels[channel].dev->CSR.OE)
    while(1);
  channels[channel].dev->BCR = divisions - 1;
  // now calculate value for the CDR
  int divisor = PM_CLK_FREQ/divisions/base_frequency;
  channels[channel].dev->CDR = divisor - 1;
  // set duty cycle to zero
  channels[channel].dev->DCR = 0;
  channels[channel].ct_set = 1;
}

// Put the channel in PDM mode.
void PM_set_PDM_mode(int channel)
{
  ASSERT(channel >= 0 && channel < NUM_CHANNELS);
  // if current task does not own the channel, lock up for debugging
  //if(channels[channel].owner != xTaskGetCurrentTaskHandle())
  //while(1);
  channels[channel].dev->CSR.PDMM = 1;
  channels[channel].mode_set = 1;
}

// Put the channel in PWM mode.
void PM_set_PWM_mode(int channel)
{
  ASSERT(channel >= 0 && channel < NUM_CHANNELS);
  // if current task does not own the channel, lock up for debugging
  //if(channels[channel].owner != xTaskGetCurrentTaskHandle())
  //while(1);
  channels[channel].dev->CSR.PDMM = 0;
  channels[channel].mode_set = 1;
}

// Enable the FIFO. 
int PM_enable_FIFO(int channel)
{
  ASSERT(channel >= 0 && channel < NUM_CHANNELS);
  // if current task does not own the channel, lock up for debugging
  //if(channels[channel].owner != xTaskGetCurrentTaskHandle())
  //while(1);
  channels[channel].dev->CSR.SLFM = 1;
  channels[channel].dev->CSR.FIL = 8;
  channels[channel].fifo_set = 1;
}

// Disable the FIFO.
void PM_disable_FIFO(int channel)
{
  ASSERT(channel >= 0 && channel < NUM_CHANNELS);
  // if current task does not own the channel, lock up for debugging
  //if(channels[channel].owner != xTaskGetCurrentTaskHandle())
  //while(1);
  channels[channel].dev->CSR.SLFM = 0;
  channels[channel].fifo_set = 1;
}

// Enable output on the channel. Return zero if the channel is
// not fully configured.
int PM_enable(int channel)
{
  ASSERT(channel >= 0 && channel < NUM_CHANNELS);
  // if current task does not own the channel, lock up for debugging
  //if(channels[channel].owner != xTaskGetCurrentTaskHandle())
  //while(1);
  if(! (channels[channel].fifo_set &&
	channels[channel].mode_set &&
	channels[channel].ct_set)
     )
    return 0;
  channels[channel].dev->CSR.OE = 1;
}

// Disable output on the channel.
void PM_disable(int channel)
{
  ASSERT(channel >= 0 && channel < NUM_CHANNELS);
  // if current task does not own the channel, lock up for debugging
  //if(channels[channel].owner != xTaskGetCurrentTaskHandle())
  //while(1);
  channels[channel].dev->CSR.OE = 0;
}

// Set the duty cycle for the channel.  If the channel is in 
// FIFO mode, this function writes to the FIFO.  Otherwise, 
// it writes directly to the Duty Cycle Register.
void PM_set_duty(int channel,int duty)
{
  ASSERT(channel >= 0 && channel < NUM_CHANNELS);
  // if current task does not own the channel, lock up for debugging
  //if(channels[channel].owner != xTaskGetCurrentTaskHandle())
  //  while(1);
  channels[channel].dev->DCR = duty;
}

// If the channel is in FIFO mode, this function returns 1 if
// the FIFO is full. In all other cases, it returns zero.
int PM_FIFO_full(int channel)
{
  ASSERT(channel >= 0 && channel < NUM_CHANNELS);
  // if current task does not own the channel, lock up for debugging
  // if(channels[channel].owner != xTaskGetCurrentTaskHandle())
  // while(1);

  return channels[channel].dev->CSR.FF;
}


