// The audio backend contains the ISR that runs the audio Pulse
// Modulator M and the audio mixer task.  The audio ISR is written for
// the LDP-001 PM device driver.  If you use a different Pulse
// Modulator, you may have to write a compatible driver.

#include <FreeRTOSConfig.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <ks.h>

// The Pulse Modulator has multiple channels.  My standard setup puts
// the audio amplifier of the NEXYS A7 on channel 0
#define AUDIO_PM_CHANNEL 0
// Bit depth for the PM device
#define DEPTH BIT_DEPTH
// Sample rate for the PM device
#define FREQ  SAMPLE_RATE
// Number of buffers for round-robin from the mixer to the PM ISR
#define NUM_MIXER_BUFFERS 4
// Number of samples in each audio buffer
#define AUDIO_BUFFER_SIZE AUDIO_BUFFER_LENGTH
// Number of queues going into the mixer
#define NUM_AUDIO_QUEUES 2

// main should call this function to set up the audio and start the
// mixer
void audio_init();

// Audio tasks can send data to the mixer through their audio queue.
extern QueueHandle_t audio_queue[NUM_AUDIO_QUEUES];
extern QueueHandle_t audio_return_queue[NUM_AUDIO_QUEUES];

extern TaskHandle_t mixer_task_handle;

