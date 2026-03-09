#ifndef LDP_001_PM_DRIVER_H
#define LDP_001_PM_DRIVER_H

#include <FreeRTOSConfig.h>
#include <FreeRTOS.h>

// Interrupt service routine for the PM driver
void PM_handler();

// Reset all PM channel hardware to a safe state.
// No FreeRTOS dependencies — safe to call before the scheduler starts.
void PM_reset_hardware();

// Initialize the driver
void PM_init();

// Get exclusive access to the pulse modulator channel.
BaseType_t PM_acquire(int channel);

// Release the channel. This function also disables the channel.
void PM_release(int channel);

// Set the interrupt handle function for the channel.
void PM_set_handler(int channel, void (*handler)(BaseType_t *HPTW));

// Set the base frequency and number of divisions for the
// channel.
void PM_set_cycle_time(int channel, int divisions, int base_frequency);

// Put the channel in PDM mode.
void PM_set_PDM_mode(int channel);

// Put the channel in PWM mode.
void PM_set_PWM_mode(int channel);

// Enable the FIFO. Return zero if the handler, cycle time, or
// mode have not been set.
int PM_enable_FIFO(int channel);

// Disable the FIFO.
void PM_disable_FIFO(int channel);

// Enable output on the channel. Return zero if the channel is
// not fully configured.
int PM_enable(int channel);

// Disable output on the channel.
void PM_disable(int channel);

// Set the duty cycle for the channel.  If the channel is in 
// FIFO mode, this function writes to the FIFO.  Otherwise, 
// it writes directly to the Duty Cycle Register.
void PM_set_duty(int channel,int duty);

// If the channel is in FIFO mode, this function returns 1 if
// the FIFO is full. In all other cases, it returns zero.
int PM_FIFO_full(int channel);

#endif
