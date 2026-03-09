// The audio backend contains the ISR that runs the audio Pulse
// Modulator M and the audio mixer task.  The audio ISR is written for
// the LDP-001 PM device driver.  If you use a different Pulse
// Modulator, you may have to write a compatible driver.

#include <FreeRTOSConfig.h>
#include <FreeRTOS.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <queue.h>
#include <LDP-001_PM_driver.h>
#include <audio_backend.h>

// Each audio queue will have room for this many audio buffer pointers.
#define NUM_AUDIO_BUFFERS 4

#define MIXER_STACK_SIZE 1024
TaskHandle_t mixer_task_handle;
static StackType_t  mixer_task_stack[MIXER_STACK_SIZE];
static StaticTask_t mixer_TCB;

// Each audio task will send audio buffers (actually just pointers) to
// the mixer using a dedicated queue.  The mixer will combine the
// outputs of the audio producing tasks and send the results to the
// ISR.

// The mixer will send buffers to the ISR.
static QueueHandle_t MixerToISRqueue;
// The ISR will return empty buffers to the mixer.
static QueueHandle_t ISRToMixerqueue;

// Allocate outgoing and return queue for the mixer and ISR.
static StaticQueue_t MixerToISRqueue_QCB, ISRToMixerqueue_QCB;
// Allocate data storage for the two queues
static uint16_t *MixerToISRqueue_buf[NUM_MIXER_BUFFERS];
static uint16_t *ISRToMixerqueue_buf[NUM_MIXER_BUFFERS];
// Allocate the buffers to be shared by the Mixer and ISR.
static uint16_t mixer_buffers[NUM_AUDIO_BUFFERS][AUDIO_BUFFER_SIZE];


// Allocate an outgoing queue and a return queue for each audio
// producer task to send data to the mixer and get back the empty
// buffers (don't want to use dynamic memory to allocate the buffers,
// so we just preallocate them and track the pointers using a pair of
// queues.
QueueHandle_t audio_queue[NUM_AUDIO_QUEUES];
QueueHandle_t audio_return_queue[NUM_AUDIO_QUEUES];

// Allocate all of the outgoing and return queues for audio producers
// to send data to the mixer
static StaticQueue_t audio_queue_QCB[NUM_AUDIO_QUEUES];
static StaticQueue_t audio_return_queue_QCB[NUM_AUDIO_QUEUES];
// Allocate data storage for the queues
static int16_t* audio_queue_buf[NUM_AUDIO_QUEUES][AUDIO_BUFFER_SIZE];
static int16_t* audio_return_queue_buf[NUM_AUDIO_QUEUES][AUDIO_BUFFER_SIZE];
// Allocate the data buffers to be shared between audio producers and the mixer
static int16_t audio_buffers[NUM_AUDIO_QUEUES][NUM_AUDIO_BUFFERS][AUDIO_BUFFER_SIZE];

// The interrupt handler for the audio pulse modulator
void audio_handler(BaseType_t *HPTW)
{
  static uint16_t *buffer = NULL; // make it static so that it always exists.
  static int buffer_pos = 0;
  static BaseType_t status = pdFALSE;
  static int last_value = 0;
  // if buffer == null, then get a buffer from the queue.
  if(buffer == NULL)
    status = xQueueReceiveFromISR(MixerToISRqueue,&buffer,HPTW);
  
  while(!PM_FIFO_full(AUDIO_PM_CHANNEL))
    {
      // You may not be able to get a buffer (the mixer may not
      // have run yet) In that case, fill the FIFO with zeros.
      if(status == pdFALSE) 
        PM_set_duty(AUDIO_PM_CHANNEL,last_value);
      else
        {
          // Transfer data from the buffer to the FIFO
          last_value = buffer[buffer_pos++];
          PM_set_duty(AUDIO_PM_CHANNEL,last_value);
          if(buffer_pos >= AUDIO_BUFFER_SIZE)
            {
              // try to get another buffer
              buffer_pos = 0;
              // send current buffer back to mixer
              xQueueSendFromISR(ISRToMixerqueue,&buffer,HPTW);
              // get a new buffer from the mixer
              status = xQueueReceiveFromISR(MixerToISRqueue,&buffer,HPTW);
              if(status == pdFALSE)
                buffer = NULL;
            }
        }
    }
}

// The mixer task receives data from the individual audio tasks, and
// mixes the audio data before sending it to the ISR.
static void mixer_task(void *params)
{
  int i,j;
  uint16_t *buffer;
  int16_t *ebuf;
  int tmp;
  static int32_t tmpbuffer[AUDIO_BUFFER_SIZE];

  // configure and enable the pulse modulator
  PM_acquire(AUDIO_PM_CHANNEL);
  PM_set_cycle_time(AUDIO_PM_CHANNEL,1<<DEPTH,FREQ);
  PM_set_PDM_mode(AUDIO_PM_CHANNEL);
  PM_enable_FIFO(AUDIO_PM_CHANNEL);
  PM_set_handler(AUDIO_PM_CHANNEL,&audio_handler);
  PM_enable(AUDIO_PM_CHANNEL);

  while (1)
    {

      // Do a blocking call to get a buffer from the PM_to_mixer queue
      xQueueReceive(ISRToMixerqueue,&buffer,portMAX_DELAY);

      // clear tmpbuffer
      bzero(tmpbuffer,sizeof(int32_t)*AUDIO_BUFFER_SIZE);

      // Get buffers from audio channels and combine them to create
      // an output buffer
      for(i=0;i<NUM_AUDIO_QUEUES;i++)
        {
          if(xQueueReceive(audio_queue[i],&ebuf,0) == pdTRUE)
            {
              // add the data to our outgoing buffer
              for(j=0;j<AUDIO_BUFFER_SIZE;j++)
                {
                  tmp = (int)ebuf[j];
                  tmpbuffer[j] += tmp;
                }
              // send the empty buffer back
              xQueueSend(audio_return_queue[i],&ebuf,portMAX_DELAY);
            }
        }

      // Convert from signed to unsigned, clipping to PM range [0, 2^DEPTH - 1]
      for(j=0;j<AUDIO_BUFFER_SIZE;j++)
        {
          tmp = (tmpbuffer[j]>>2) + (1<<(DEPTH-1));
          if(tmp > ((1<<DEPTH)-1))
            tmp = (1<<DEPTH)-1;
          else if(tmp < 0)
            tmp = 0;
          buffer[j] = (uint16_t)tmp;
        }
      
      // Send the buffer to the mixer_to_PM queue
      xQueueSend(MixerToISRqueue,&buffer,portMAX_DELAY);
    }
}


// main should call this function to set up the audio and start the
// mixer
void audio_init() 
{
  int i,j;
  uint16_t *buffer;

  // create all of the queues that will be used by the audio tasks to
  // send data to the mixer. Store their handles in the audio_quee and
  // audio_return_queue arrays
  for(i=0;i<NUM_AUDIO_QUEUES;i++)
    {
      audio_queue[i] = xQueueCreateStatic(NUM_AUDIO_BUFFERS,
                                          sizeof(uint8_t*),
                                          (uint8_t *)audio_queue_buf[i],
                                          &audio_queue_QCB[i]);
      audio_return_queue[i] = xQueueCreateStatic(NUM_AUDIO_BUFFERS,
                                                 sizeof(uint8_t*),
                                                 (uint8_t *)audio_return_queue_buf[i],
                                                 &audio_return_queue_QCB[i]);
      // Fill the audio return queue by putting the pointers to the
      // audio_buffers in it.
      for(j=0;j<NUM_AUDIO_BUFFERS;j++)
        {
          buffer = audio_buffers[i][j];
          xQueueSend(audio_return_queue[i],&buffer,0);
        }
    }
  
  // create the two queues to communicate between the mixer and the ISR
  MixerToISRqueue = xQueueCreateStatic(NUM_MIXER_BUFFERS,
                                       sizeof(uint16_t*),
                                       (uint8_t *)MixerToISRqueue_buf,
                                       &MixerToISRqueue_QCB);
  ISRToMixerqueue = xQueueCreateStatic(NUM_MIXER_BUFFERS,
                                       sizeof(uint16_t*),
                                       (uint8_t *)ISRToMixerqueue_buf,
                                       &ISRToMixerqueue_QCB);
  
  // Create the audio buffers for communication between the mixer and ISR

  // put the pointers to the mixer_buffers in the PM_to_mixer queue
  for(i=0;i<NUM_MIXER_BUFFERS;i++)
    {
      buffer = mixer_buffers[i];
      xQueueSend(ISRToMixerqueue,&buffer,0);
    }
  
  // Create the mixer task.
  mixer_task_handle = xTaskCreateStatic(mixer_task,"audio mixer",
                                        MIXER_STACK_SIZE,
                                        NULL,9,mixer_task_stack,&mixer_TCB);

}

