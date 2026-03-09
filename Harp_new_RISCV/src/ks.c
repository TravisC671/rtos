/* This code produces music using the Karplus-Strong algorithm.
   Author: Larry Pyeatt
   Date: 11-1-2012 (revised 9-6-2021)
   (C) All rights reserved

   This code uses the Karplus-Strong algorithm to simulate a
   stringed instrument.  It sounds similar to a harp.

*/
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ks.h>

/* if NOTE is a midi number, then the frequency is:
   hz = 440*pow(2,(NOTE-69.0)/12.0), and the array size needed is
   SAMPLE_RATE/Hz This lookup table translates midi numbers to the
   array size.  The first midi number is 21, which is A0 (A at
   octave 0).  The two lowest octaves take too much space, and the
   upper octaves become inaccurate, so it is best to take a few
   octaves from the middle. A good range for 36621 Hz sample rate
   MIDI notes 28 through 88.
*/

// The Karplus-Strong buffers starting at midi note 28
#define FIRST_MIDI 28
#define LAST_MIDI 88
#define NUM_STRINGS (LAST_MIDI - FIRST_MIDI + 1)

int16_t array_size[] = {
		889,839,792,747,705,666,628,593,560,528,499,471,
		444,419,396,374,353,333,314,297,280,264,249,235,
		222,210,198,187,176,166,157,148,140,132,125,118,
		111,105,99,93,88,83,79,74,70,66,62,59,
		56,52,49,47,44,42,39,37,35,33,31,29,
		28};

typedef struct{
  // array for keeping track of our position in each buffer.
  int16_t position[NUM_STRINGS];
  // array for timeouts for each buffer.
  int32_t timer[NUM_STRINGS];
  // active string tracking: only iterate strings that are sounding
  int16_t active_list[NUM_STRINGS];
  int16_t num_active;
  int16_t E1[889];
  int16_t F1[839];
  int16_t F1s[792];
  int16_t G1[747];
  int16_t G1s[705];
  int16_t A1[666];
  int16_t A1s[628];
  int16_t B1[593];
  int16_t C2[560];
  int16_t C2s[528];
  int16_t D2[499];
  int16_t D2s[471];
  int16_t E2[444];
  int16_t F2[419];
  int16_t F2s[396];
  int16_t G2[374];
  int16_t G2s[353];
  int16_t A2[333];
  int16_t A2s[314];
  int16_t B2[297];
  int16_t C3[280];
  int16_t C3s[264];
  int16_t D3[249];
  int16_t D3s[235];
  int16_t E3[222];
  int16_t F3[210];
  int16_t F3s[198];
  int16_t G3[187];
  int16_t G3s[176];
  int16_t A3[166];
  int16_t A3s[157];
  int16_t B3[148];
  int16_t C4[140];
  int16_t C4s[132];
  int16_t D4[125];
  int16_t D4s[118];
  int16_t E4[111];
  int16_t F4[105];
  int16_t F4s[99];
  int16_t G4[93];
  int16_t G4s[88];
  int16_t A4[83];
  int16_t A4s[79];
  int16_t B4[74];
  int16_t C5[70];
  int16_t C5s[66];
  int16_t D5[62];
  int16_t D5s[59];
  int16_t E5[56];
  int16_t F5[52];
  int16_t F5s[49];
  int16_t G5[47];
  int16_t G5s[44];
  int16_t A5[42];
  int16_t A5s[39];
  int16_t B5[37];
  int16_t C6[35];
  int16_t C6s[33];
  int16_t D6[31];
  int16_t D6s[29];
  int16_t E6[28];
} ks_struct;

// an array of offsets to the buffers, so that we can access them
// quickly and easily using the midi number.
static unsigned bufferoffset[] = {
    offsetof(ks_struct,E1),offsetof(ks_struct,F1),offsetof(ks_struct,F1s),
    offsetof(ks_struct,G1),offsetof(ks_struct,G1s),offsetof(ks_struct,A1),
    offsetof(ks_struct,A1s),offsetof(ks_struct,B1),offsetof(ks_struct,C2),
    offsetof(ks_struct,C2s),offsetof(ks_struct,D2),offsetof(ks_struct,D2s),
    offsetof(ks_struct,E2),offsetof(ks_struct,F2),offsetof(ks_struct,F2s),
    offsetof(ks_struct,G2),offsetof(ks_struct,G2s),offsetof(ks_struct,A2),
    offsetof(ks_struct,A2s),offsetof(ks_struct,B2),offsetof(ks_struct,C3),
    offsetof(ks_struct,C3s),offsetof(ks_struct,D3),offsetof(ks_struct,D3s),
    offsetof(ks_struct,E3),offsetof(ks_struct,F3),offsetof(ks_struct,F3s),
    offsetof(ks_struct,G3),offsetof(ks_struct,G3s),offsetof(ks_struct,A3),
    offsetof(ks_struct,A3s),offsetof(ks_struct,B3),offsetof(ks_struct,C4),
    offsetof(ks_struct,C4s),offsetof(ks_struct,D4),offsetof(ks_struct,D4s),
    offsetof(ks_struct,E4),offsetof(ks_struct,F4),offsetof(ks_struct,F4s),
    offsetof(ks_struct,G4),offsetof(ks_struct,G4s),offsetof(ks_struct,A4),
    offsetof(ks_struct,A4s),offsetof(ks_struct,B4),offsetof(ks_struct,C5),
    offsetof(ks_struct,C5s),offsetof(ks_struct,D5),offsetof(ks_struct,D5s),
    offsetof(ks_struct,E5),offsetof(ks_struct,F5),offsetof(ks_struct,F5s),
    offsetof(ks_struct,G5),offsetof(ks_struct,G5s),offsetof(ks_struct,A5),
    offsetof(ks_struct,A5s),offsetof(ks_struct,B5),offsetof(ks_struct,C6),
    offsetof(ks_struct,C6s),offsetof(ks_struct,D6),offsetof(ks_struct,D6s),
    offsetof(ks_struct,E6)};

// Create two ks generators
static ks_struct ks_chan[2];

static int16_t* get_bufptr(int ks_channel, int index)
{
  uint8_t *tmpptr;
  // get pointer to the channel structure
  tmpptr = (uint8_t *)(ks_chan + ks_channel);
  // add offset to correct array within the structure
  tmpptr += (bufferoffset[index]);
  return (int16_t *)tmpptr;
}

// Plucking a string is simulated by filling the array with
// random numbers between -0.5 and +0.5. volume is an S(0,16).
void ks_string_pluck(int ks_channel, int16_t midi_num, int16_t volume)
{
  int index;
  int32_t tmp;   // using 32 bits here makes the multiply work
  int16_t tmp16;
  int i;
  int16_t *buffer;
  int already_active;
  if((midi_num >= FIRST_MIDI)&&(midi_num <= LAST_MIDI))
    {
      index = midi_num - FIRST_MIDI;
      buffer = get_bufptr(ks_channel,index);
      for(i=0;i<array_size[index];i++)
        {
          // Create a random S(15,16) between -0.5 and 0.5.
          // Get a 32 bit random number.
          tmp = rand();
          // Convert the random number to S(0,15) between -0.5 and 0.5
          tmp16 = tmp & 0xFFFF;
          tmp16 >>= 2;
          // multiply by volume, convert result to S(0,15)
          tmp = (volume * (int32_t)tmp16)>>16;
          // store in buffer
          buffer[i] = tmp;
        }
      ks_chan[ks_channel].timer[index]=40*array_size[index];
      // Add to active list if not already there
      already_active = 0;
      for(i=0;i<ks_chan[ks_channel].num_active;i++)
        {
          if(ks_chan[ks_channel].active_list[i] == index)
            {
              already_active = 1;
              i = ks_chan[ks_channel].num_active; // exit loop
            }
        }
      if(!already_active)
        {
          ks_chan[ks_channel].active_list[ks_chan[ks_channel].num_active] = index;
          ks_chan[ks_channel].num_active++;
        }
    }
}

#ifdef GET_KS_MIN_MAX
int ks_min_smpl = INT_MAX;
int ks_max_smpl = INT_MIN;
#endif

void ks_fill_buffer(int ks_channel, int16_t *data)
{
  int32_t tmp_buffer[AUDIO_BUFFER_LENGTH];
  int i,k;
  int32_t temp;
  int nextpos;
  int16_t *buffer;
  int32_t value;
  int32_t fact = 0xFEFE;  // a U(0,16) that is slightly less than 1.0
  int32_t cposition;
  int32_t csize;
  int j;
  // clear the temporary buffer
  memset(tmp_buffer,0,sizeof(int32_t) * AUDIO_BUFFER_LENGTH);
  // generate some audio samples for each active string,
  // adding them together in tmp_buffer.
  // Only iterate over strings in the active list.
  k = 0;
  while(k < ks_chan[ks_channel].num_active)
    {
      j = ks_chan[ks_channel].active_list[k];
      if(ks_chan[ks_channel].timer[j] > 0)
	 {
	   cposition = ks_chan[ks_channel].position[j];
	   csize = array_size[j];
        buffer = get_bufptr(ks_channel,j);
	   for(i=0; i < AUDIO_BUFFER_LENGTH; i++)
	     {
	        // update each active string and add its output to the sum
 	        nextpos = cposition + 1;
	        if(nextpos >= csize)
		    nextpos = 0;
	        // calculate value as an S(0,31)
 	        value=(fact*((int32_t)buffer[cposition] +
                          (int32_t)buffer[nextpos])+1)>>1;
 	        value >>= 16;  // convert to S(0,15)
 	        buffer[cposition] = value;
 	        tmp_buffer[i] += value;
 	        cposition = nextpos;
#ifdef GET_KS_MIN_MAX
             ks_min_smpl = (value < ks_min_smpl)?value:ks_min_smpl;
             ks_max_smpl = (value > ks_max_smpl)?value:ks_max_smpl;
#endif
 	    }
 	  ks_chan[ks_channel].position[j] = cposition;
 	  ks_chan[ks_channel].timer[j]--;
	  k++;
 	}
      else
	{
	  // Timer expired: remove from active list by replacing
	  // with last element.
	  ks_chan[ks_channel].num_active--;
	  ks_chan[ks_channel].active_list[k] =
	    ks_chan[ks_channel].active_list[ks_chan[ks_channel].num_active];
	  // Don't increment k; re-examine this slot.
	}
    }
  // copy the results from tmp_buffer to data, clipping to int16_t range.
  for(i=0; i < AUDIO_BUFFER_LENGTH; i++)
    {
      temp = tmp_buffer[i];
      if(temp > 32767)
        temp = 32767;
      else if(temp < -32768)
        temp = -32768;
      data[i] = (int16_t)temp;
    }
}
