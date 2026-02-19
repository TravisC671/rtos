#ifndef AXI_TIMER_H
#define AXI_TIMER_H

// If you add more AXI_timer devices to the design, then change this
// define.  Each device supports two timers, and we currently have two
// devices.
#define NUM_AXI_TIMERS 4

// Our timers are driven by a 50 MHz clock. (The LL gives us 64 bits
// to work with when calculating the count value.
#define AXI_TIMER_CLOCK_FREQ 50000000LL

// Provide a convenience macro to convert microseconds into timer
// counts.
#define AXI_TIMER_US_TO_COUNT(x) \
  (((x*AXI_TIMER_CLOCK_FREQ)/1000000)-2)

// Provide a convenience macro to convert Hertz into timer
// counts.
#define AXI_TIMER_HZ_TO_COUNT(x) \
  ((AXI_TIMER_CLOCK_FREQ/x)-2)

// In the following functions, "timer" refers to the specific timer
// that you are working with, and must be between 0 and (NUM_AXI_TIMERS-1)
// inclusive.

// Allocate a timer.  Returns -1 if no timers are available.
int AXI_TIMER_allocate();

// Release a timer. 
void AXI_TIMER_free(unsigned int timer);

// Assign a functon to handle interrupts for the given timer. The
// assigned function will be called when the given timer generates an
// interrupt.
void AXI_TIMER_set_handler(unsigned int timer, void (*handler)());

// start the timer
void AXI_TIMER_enable(unsigned int timer);   

// stop the timer. If remove_handler != 0 then the hander is also
// unset
void AXI_TIMER_disable(unsigned int timer, int remove_handler);

// enable the timer to generate interrupts
void AXI_TIMER_enable_interrupt(unsigned int timer); 

// Disable the timer interrupt  
void AXI_TIMER_disable_interrupt(unsigned int timer);

// In the following two functions, value is set in clock cycles. Use
// AXI_TIMER_US_TO_COUNT to convert desired nanoseconds to desired timer
// count value, or uset AXI_TIMER_HZ_TO_COUNT to set it in Hertz.

// Configure and start the timer to give repeating interrupts.
void AXI_TIMER_set_repeating(unsigned int timer, int count);

// Configure and start the timer give a single interrupt and then stop.
void AXI_TIMER_set_oneshot(unsigned int timer, int count);

// The timers also have a PWM mode, which we may want to support
// someday. These are are a couple of functions that may be useful at
// that point, but we are not going to implement them now.
// void AXI_TIMER_set_count_up(int timer);
// void AXI_TIMER_set_count_down(int timer);


#endif
