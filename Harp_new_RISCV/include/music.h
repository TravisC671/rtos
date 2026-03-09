#ifndef MUSIC_H
#define MUSIC_H
#include <FreeRTOSConfig.h>
#include <FreeRTOS.h>

#define MIDI_RECEIVER_STACK_SIZE 1024
#define MUSIC_PRODUCER_STACK_SIZE 1024

// interrupt handler for the pushbuttons (used to enable/disable autoplay */
void button_handler();

/* Structure that will hold the TCB of the task being created. */
extern StaticTask_t midi_receiver_TCB;

/* Buffer that the task being created will use as its stack. Note this
is an array of StackType_t variables. The size of StackType_t is
dependent on the RTOS port. */
extern StackType_t midi_receiver_stack[ MIDI_RECEIVER_STACK_SIZE ];


/* Structure that will hold the TCB of the task being created. */
extern StaticTask_t music_producer_TCB;

/* Buffer that the task being created will use as its stack. Note this
is an array of StackType_t variables. The size of StackType_t is
dependent on the RTOS port. */
extern StackType_t music_producer_stack[ MUSIC_PRODUCER_STACK_SIZE ];

void music_init();

void midi_receiver_task(void *params);

// The music_init function sets up the timer and sets the handler to
// music_consumer_ISR
void music_init();

// Task to generate the audio samples
void music_producer_task(void *params);

// By default the music producer sends random songs from the music
// database. Calling this function will shut off the music, or resume
// it where it left off, depending on the current state.  The keyboard
// task will call this function when F1 is pressed.
void music_toggle();

// By default, the MIDI input is enabled.  The init task should
// disable it by calling this function.  The keyboard task will call
// this function when F2 is pressed.
void MIDI_toggle();

#endif

