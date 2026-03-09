#ifndef KS_H
#define KS_H

#include <stdint.h>

/* If we change the sample rate, we have to re-calculate the
   lengths of all of the "string" buffers in ks.c */
#define SAMPLE_RATE 36621
#define BIT_DEPTH 12
#define AUDIO_BUFFER_LENGTH 64

/* This function simulates plucking one string */
void ks_string_pluck(int ks_channel, int16_t midi_num, int16_t volume);

/* This function fills a buffer with audio samples. */
void ks_fill_buffer(int ks_channel, int16_t *data);

#endif
