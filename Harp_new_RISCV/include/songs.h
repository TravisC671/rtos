#ifndef SONGS_H
#define SONGS_H

// This file provides access to the songs.  A song is an array of
// note_t where the last note has a value of -1. This file
// provides the data and one function to pick a random song.

// This struct defines a note.  The time is given in milliseconds from
// the beginning of the song.  The note is a midi note value, and
// volume is given as a fixed point U(16,8).  A song is just a
// (variable length) array of notes, where the 'note' field of the
// last note is -1.  That note is not actually played, but the 'time'
// field allows us to wait for the sound from the previous note to end
// before starting the next song.

#include <stdint.h>

typedef struct{
  uint16_t time;
  int16_t note;
  uint16_t volume;
}note_t;

// Return a pointer to an array of note_t.  The last note in the array
// will have note=-1;
const note_t *get_random_song(int *tempo, const char **title);

#endif

