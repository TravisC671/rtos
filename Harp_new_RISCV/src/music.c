// include stuff from FreeRTOS
#include <FreeRTOS.h>
#include <limits.h>
#include <stdio.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>

#include <music.h>
#include <songs.h>
#include <ks.h>
#include <device_addrs.h>
#include <AXI_timer.h>
#include <UART_16550.h>
#include <ANSI_terminal.h>
#include <audio_backend.h>
#include <LDP-001_PM_driver.h>

#define CHANNEL 0

// The idea here is to create a producer/consumer problem using two
// queues of buffers. The buffers are statically allocated, and are
// used Round-Robin. This is actually easier than it may sound.

// The consumer is an ISR. It takes a pointer to a buffer from the
// queue, and sends each item to the PWM until the buffer is
// empty. When the buffer is empty, it sends it back to the producer
// through a second queue.

// The producer takes a pointer to an empty empty buffer from the
// second queue, fills the buffer with audio samples, then puts the
// pointer on the queue for the consumer.

static SemaphoreHandle_t music_semaphore;
static StaticSemaphore_t music_semaphore_buffer;


static int MIDIenabled = 1;

/* Structure that will hold the TCB of the task being created. */
StaticTask_t midi_receiver_TCB;

/* Buffer that the task being created will use as its stack. Note this
is an array of StackType_t variables. The size of StackType_t is
dependent on the RTOS port. */
StackType_t midi_receiver_stack[ MIDI_RECEIVER_STACK_SIZE ];


/* Structure that will hold the TCB of the task being created. */
StaticTask_t music_producer_TCB;

/* Buffer that the task being created will use as its stack. Note this
is an array of StackType_t variables. The size of StackType_t is
dependent on the RTOS port. */
StackType_t music_producer_stack[ MUSIC_PRODUCER_STACK_SIZE ];

#define MUSIC_CHAN 0
#define MIDI_CHAN 1

void midi_receiver_task(void *params)
{
  char buffer[32];
  char ch,MIDI_num,volume;
  do
    {
      UART_16550_get_char(UART1,&ch,portMAX_DELAY);//pdMS_TO_TICKS(100)); 
      sprintf(buffer,"MIDI message: %X",ch);
      UART_16550_tx_lock(UART0,portMAX_DELAY);
      ANSI_moveTo(UART0,1,0);
      UART_16550_write_string(UART0,buffer,portMAX_DELAY);
      ANSI_cleartoeol(UART0);
      UART_16550_tx_unlock(UART0);
      switch((ch&0xF0)>>4)
        {
          // channel voice messages
        case 0x9: // note on
          UART_16550_get_char(UART1,&MIDI_num,portMAX_DELAY);
          MIDI_num &= 0x7F;
          UART_16550_get_char(UART1,&volume,portMAX_DELAY);
          if(MIDIenabled)
            ks_string_pluck(MIDI_CHAN,MIDI_num,volume<<7);
          // for debugging, echo to the terminal
          sprintf(buffer,"On: %d %d",MIDI_num,volume<<7);
          UART_16550_tx_lock(UART0,portMAX_DELAY);
          ANSI_moveTo(UART0,2,0);
          UART_16550_write_string(UART0,buffer,portMAX_DELAY);
          ANSI_cleartoeol(UART0);
          UART_16550_tx_unlock(UART0);
          break;
        case 0x8: // note off
          UART_16550_get_char(UART1,&MIDI_num,portMAX_DELAY);
          UART_16550_get_char(UART1,&volume,portMAX_DELAY);
          //      sprintf(buffer,"Off: %d %d\n\r",MIDI_num,volume<<7);
          //	  UART_16550_lock(UART0);
          //	  moveTo(UART0,4,0);
          //      UART_16550_write_string(UART0,buffer);
          //	  cleartoeol(UART0);
          //      UART_16550_unlock(UART0);
          break;
          // all other messages
        case 0xA: // Polyphonic key pressure
        case 0xB: // Control change
        case 0xC: // Program change
        case 0xD: // Channel pressure
        case 0xE: // Pitch Blend
        default:
          UART_16550_tx_lock(UART0,portMAX_DELAY);
          ANSI_moveTo(UART0,2,0);
          UART_16550_write_string(UART0,"unknown MIDI event",portMAX_DELAY);
          ANSI_cleartoeol(UART0);
          ANSI_moveTo(UART0,3,0);
          ANSI_cleartoeol(UART0);
          UART_16550_tx_unlock(UART0);
          //while(1);
        }
    }while(1);
}


#define PRODUCER_QUEUE 0

void music_producer_task(void *params)
{
  int index;
  int16_t *current_buffer = NULL;
  const note_t *current_song;
  //  int tempo = 0x100;   // 1.0 stored as a U(16,8) */
  // int tempo = 0x80;   // 0.5 stored as a U(16,8) */
  int tempo = 0x180;   // 1.5 stored as a U(16,8) */
  int next_note_time;
  int current_time;
  BaseType_t xReturn;
  const char *title;
  
  UART_16550_tx_lock(UART0,portMAX_DELAY);
  ANSI_clear(UART0);
  ANSI_hideCursor(UART0);
  UART_16550_tx_unlock(UART0);

  // Start producing buffers.
  while(1)
    {
      // Pick one of the songs, and initialize the note index to the
      // beginning of the song	
      current_song = get_random_song(&tempo, (const char **)&title);
      UART_16550_tx_lock(0,portMAX_DELAY);
      ANSI_moveTo(0,0,0);
      ANSI_cleartoeol(0);
      UART_16550_write_string(0,(char *)title,portMAX_DELAY);
      UART_16550_put_char(0,'\n',portMAX_DELAY);
      UART_16550_tx_unlock(0);
      index = 0;
      current_time=0;

      // Adjust start time of next note to match the tempo. 
      // The next line is fixed point math.  It multiplies an
      // integer U(16,0) by a U(16,8) and produces a U(16,0)
      next_note_time = ((current_song[index].time * tempo) /
			AUDIO_BUFFER_LENGTH)>>3;
      do{
	// Do a semaphore take.  We will block here if we can't get
	// the semaphore.
	xSemaphoreTake(music_semaphore,portMAX_DELAY);

        // generate sound until it is time
        // to pluck the next note
        while(current_time <= next_note_time)
          {
	    current_time++;
            // get a buffer from the return queue (block if none available)
            xReturn = xQueueReceive(audio_return_queue[PRODUCER_QUEUE],
                                    &current_buffer,portMAX_DELAY);
            configASSERT(xReturn == pdPASS);
            // use the Karplus-Strong algorithm to generate another
            // millisecond of audio to fill the current buffer.  On
            // the first few calls, none of the strings have been
            // plucked, so the buffer will contain silence (or maybe
            // some final sousds from the previous song).
            ks_fill_buffer(MUSIC_CHAN,current_buffer);
            // send the current buffer to the consumer ISR. The send
            // will never block, because the queue has 16 slots, and
            // there are only 16 buffers.
            xQueueSend(audio_queue[PRODUCER_QUEUE],
                       &current_buffer,portMAX_DELAY);
          }
        // pluck the next note(s) (unless the song just ended)
        while(
              (current_song[index].note > 0)&&
              (next_note_time <= current_time))
          {
            ks_string_pluck(MUSIC_CHAN,current_song[index].note,
                            current_song[index].volume);
            index++;
	    next_note_time = ((current_song[index].time * tempo) /
			      AUDIO_BUFFER_LENGTH)>>3;
          }
	// Do a semaphore give. The pause process may take it.
	xSemaphoreGive(music_semaphore);
      } while((current_time < next_note_time)||
              (current_song[index].note > 0));
	      
    }  
}



#define MUSIC_TOGGLE_STACK_SIZE 1024
static StackType_t music_toggle_stack[MUSIC_TOGGLE_STACK_SIZE];
static StaticTask_t music_toggle_TCB;

// Enable GPIO2 Interrupts
static TaskHandle_t music_toggle_handle = NULL;

// need a process to toggle the music player
void music_toggle_task(void *params)
{
  int still_pressed, paused=0;
  BaseType_t value, status;
  while(1)
    {
      // block until notified
      status = xTaskNotifyWait( 0, ULONG_MAX, &value, portMAX_DELAY);
      if(!paused)
	{
	  // take semaphore to stop music produce
	  xSemaphoreTake(music_semaphore,portMAX_DELAY);
	  paused = 1;
	  UART_16550_tx_lock(UART0,portMAX_DELAY);
	  ANSI_moveTo(UART0,0,25);
	  UART_16550_write_string(UART0,"(paused)",portMAX_DELAY);
	  ANSI_cleartoeol(UART0);
	  UART_16550_tx_unlock(UART0);
	  *RGB_LEDS ^= 0x00000003;
	}
      else
	{
	  // give semaphore to start music producer
	  xSemaphoreGive(music_semaphore);
	  paused = 0;
	  UART_16550_tx_lock(UART0,portMAX_DELAY);
	  ANSI_moveTo(UART0,0,25);
	  ANSI_cleartoeol(UART0);
	  UART_16550_tx_unlock(UART0);
	  *RGB_LEDS ^= 0x00000003;
	}
      still_pressed = 1;
      do
	{
	  // sleep for short period.
	  vTaskDelay(pdMS_TO_TICKS( 50 ));
	  // If button is not pressed, enable the pushbutton interrupt.
	  still_pressed = *(BUTTONS) & 1;
	  if(!still_pressed)
	    *BUTTON_ier = 2;
	} while (still_pressed);
    }
}


void button_handler()
{
  static uint32_t buttons;
  BaseType_t HPTW;
  uint32_t status;
  // Read the interrupt status
  status = *(BUTTON_isr);
  // Read the pushbuttons
  buttons = *(BUTTONS);
  // If bit zero is set, disable button interrupts and signal the
  // music toggle process.  After a timeout, the music toggle process
  // will re-enable the button interrupts.
  if(buttons & 1)
    {
      *BUTTON_ier = 0;
      xTaskNotifyFromISR(music_toggle_handle, 0, eNoAction, &HPTW);
    }
  
  // Clear the interrupts
  *(BUTTON_isr) = 2;
  INTC_ClearPendingIRQ(GPIO0_IRQ);
  portYIELD_FROM_ISR(HPTW);
}




void music_init()
{
  // Enable interrupts from the pushbuttons
  *BUTTON_gier = 0x80000000;
  *BUTTON_ier = 2;
  INTC_EnableIRQ(GPIO0_IRQ);
  
  // set up the semaphore, initialize to one
  music_semaphore = xSemaphoreCreateBinaryStatic(&music_semaphore_buffer);
  xSemaphoreGive(music_semaphore);
  *RGB_LEDS |= 0x00000002;
  
  // start the music toggle task
  music_toggle_handle = xTaskCreateStatic(music_toggle_task,
					  "music_toggle",
					  MUSIC_TOGGLE_STACK_SIZE,
					  NULL,11,music_toggle_stack,
					  &music_toggle_TCB);

}





/* void music_toggle() */
/* { */
/*   playing = !playing; */
/*   UART_16550_tx_lock(UART0,portMAX_DELAY); */
/*   ANSI_moveTo(UART0,0,25); */
/*   if(!playing) */
/*     UART_16550_write_string(UART0,"(paused)",portMAX_DELAY); */
/*   ANSI_cleartoeol(UART0); */
/*   UART_16550_tx_unlock(UART0); */
/* } */
/* void MIDI_toggle() */
/* { */
/*   MIDIenabled = !MIDIenabled; */
/*   UART_16550_tx_lock(UART0,portMAX_DELAY); */
/*   ANSI_moveTo(UART0,2,0); */
/*   UART_16550_write_string(UART0,"MIDI input: ",portMAX_DELAY); */
/*   if(MIDIenabled) */
/*     UART_16550_write_string(UART0,"enabled",portMAX_DELAY); */
/*   else */
/*     UART_16550_write_string(UART0,"disabled",portMAX_DELAY); */
/*   ANSI_cleartoeol(UART0); */
/*   UART_16550_tx_unlock(UART0); */
/* } */

