// This program generates the C code to do Karplus-Strong at the given
// sample rate (frequency), for the given range of MIDI notes.

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

typedef struct{
  int number;
  char name[4];
  float frequency;
  int buffer_size;
} note_t;

void usage(char *name)
{
  printf("This program generates the code for Karplus-Strong\n");
  printf("at the given sample rate, given in kHz.\n");
  printf("USAGE: %s sample_rate starting_midi_note ending_midi_note bit_depth\n",name);
  exit(1);
}


int main(int argc, char **argv)
{
  int i=0;
  int table_size=0;
  note_t notes[200];
  float samples_per_ms;
  int rate;
  int startnote,endnote;
  FILE *table,*ksc,*ksh;
  int numonline;
  int bit_depth;
  
  
  if(argc != 5)
    usage(argv[0]);

  if((table=fopen("midi_table.txt","r")) == NULL)
    {
      printf("\nunable to open midi_table.txt\n");
      exit(2);
    }

  rate = atoi(argv[1]);

  startnote = atoi(argv[2]);
  if(startnote<21)
    {
      printf("Starting note must be >= 21.\n");
      exit(3);
    }
  printf("starting at midi note %d\n",startnote);
  endnote = atoi(argv[3]);
  if(endnote > 108)
    {
      printf("Ending note must be < 109.\n");
      exit(4);
    }
    
  printf("ending at midi note %d\n",endnote);
  
  bit_depth = atoi(argv[4]);
  if(bit_depth > 16)
    {
      printf("Bit depth must be <= 16.\n");
      exit(5);
    }
    
  printf("Generating Karplus strong for %d samples per second at %d bits.\n",rate,bit_depth);
  samples_per_ms = rate/1000.0;
  printf("That translates to %d samples per millisecond\n",(int)round(samples_per_ms));

  while(!feof(table))
    {
      if(fscanf(table,"%d%s",&(notes[table_size].number),notes[table_size].name)
         == 2)
        {
          if(notes[table_size].name[2] == '#')
            notes[table_size].name[2] = 's';
          table_size++;
        }
    }

  // now compute the frequecies/buffer lengths.
  for(i=0;i<table_size;i++)
    //notes[i].frequency = pow(2,(notes[i].number-69)/12.0) * samples_per_ms;
    notes[i].frequency = 440*pow(2,(notes[i].number-69)/12.0); 

  for(i=0;i<table_size;i++)
    notes[i].buffer_size = round(rate/notes[i].frequency);
 
  for(i=startnote-21;i<endnote-20;i++)
    printf("%d\t%s\t%f\t%d\n",notes[i].number,notes[i].name,notes[i].frequency,
           notes[i].buffer_size);

  ksc = fopen("ks.c","w");
  
  fprintf(ksc,"/* This code produces music using the Karplus-Strong algorithm.\n"
          "   Author: Larry Pyeatt\n"
          "   Date: 11-1-2012 (revised 9-6-2021)\n"
          "   (C) All rights reserved\n\n"
          "   This code uses the Karplus-Strong algorithm to simulate a\n"
          "   stringed instrument.  It sounds similar to a harp.\n\n"
          "*/\n"
          "#include <stddef.h>\n"
          "#include <stdlib.h>\n"
          "#include <string.h>\n"
	  "#include <limits.h>\n"
          "#include <ks.h>\n\n");

  fprintf(ksc,"/* if NOTE is a midi number, then the frequency is: \n"
          "   hz = 440*pow(2,(NOTE-69.0)/12.0), and the array size needed is\n"
          "   SAMPLE_RATE/Hz This lookup table translates midi numbers to the\n"
          "   array size.  The first midi number is 21, which is A0 (A at\n"
          "   octave 0).  The two lowest octaves take too much space, and the\n"
          "   upper octaves become inaccurate, so it is best to take a few\n"  
          "   octaves from the middle. A good range for 30 KhZ sample rate\n"
	  "   MIDI notes 28 through 88 (five octaves).\n*/\n\n");

  fprintf(ksc,"// The Karplus-Strong buffers starting at midi note %d\n",startnote);
  fprintf(ksc,"#define FIRST_MIDI %d\n",startnote);
  fprintf(ksc,"#define LAST_MIDI %d\n\n",endnote);

  // Generate the array containing the sizes of all the buffers
  fprintf(ksc,"int16_t array_size[] = {\n");
  numonline = 0;
  for(i=startnote-21;i<endnote-20;i++)
    {
      if(numonline == 0)
        fprintf(ksc,"\t\t");
      fprintf(ksc,"%d",notes[i].buffer_size);
      if(i < endnote-21)
        fprintf(ksc,",");
      if(++numonline==12)
        {
          fprintf(ksc,"\n");
          numonline = 0;
        }
    }
  fprintf(ksc,"};\n\n");

  // Generate the ks_struct that contains the buffers and other data
  // needed for the Karplus Strong algorithm.
  fprintf(ksc,"typedef struct{\n",endnote);
  fprintf(ksc,"  // array for keeping track of our position in each buffer.\n"
          "  int16_t position[%d];\n",endnote-startnote+1);
  fprintf(ksc,"  // array for timeouts for each buffer.\n"
          "  int32_t timer[%d];\n",endnote-startnote+1);
  for(i=startnote-21;i<endnote-20;i++)
    fprintf(ksc,"  int16_t %s[%d];\n",notes[i].name,notes[i].buffer_size);
  fprintf(ksc,"} ks_struct;\n",endnote);

  // Generate an array containing the byte offsets for each of the
  // buffers in the ks_struct.
  fprintf(ksc,"\n// an array of offests to the buffers, so that we "
	  "can access them\n// quickly and easily using the midi number.\n"
          "static unsigned bufferoffset[] = {\n");
  numonline = 0;
  for(i=startnote-21;i<endnote-20;i++)
    {
      if(numonline == 0)
        fprintf(ksc,"    ");
      fprintf(ksc,"offsetof(ks_struct,%s)",notes[i].name);
      if(i < endnote-21)
        fprintf(ksc,",");
      if(++numonline==3)
        {
          fprintf(ksc,"\n");
          numonline = 0;
        }
    }
  fprintf(ksc,"};\n\n");

  // The default is to set up two ks generators
  fprintf(ksc,"// Create two ks generators\n");
  fprintf(ksc,"static ks_struct ks_chan[2] = {0};\n\n");

  fprintf(ksc,
	  "static int16_t* get_bufptr(int ks_channel, int index)\n"
	  "{\n"
	  "  uint8_t *tmpptr;\n"
	  "  // get pointer to the channel structure\n"
	  "  tmpptr = (uint8_t *)(ks_chan + ks_channel);\n"
	  "  // add offset to correct array within the strucure\n"
	  "  tmpptr += (bufferoffset[index]);\n"
	  "  return (int16_t *)tmpptr;\n"
	  "}\n");

  
  fprintf(ksc,
	  "// Plucking a string is simulated by filling the array with\n"
          "// random numbers between -0.5 and +0.5. volume is an S(0,16).\n"
          "void ks_string_pluck(int ks_channel, int16_t midi_num, int16_t volume)\n"
          "{\n"
          "  int index;\n"
          "  int32_t tmp;   // using 32 bits here makes the multiply work\n"
          "  int16_t tmp16;\n"
	  "  int i;\n"
	  "  int16_t *buffer;\n"
	  "  if((midi_num >= FIRST_MIDI)&&(midi_num <= LAST_MIDI))\n"
	  "    {\n"
	  "      index = midi_num - FIRST_MIDI;\n"
	  "      buffer = get_bufptr(ks_channel,index);\n"
	  "      for(i=0;i<array_size[index];i++)\n"
          "        {\n"
          "          // Create a random S(15,16) between -0.5 and 0.5.\n"
          "          // Get a 32 bit random number.\n"
          "          tmp = rand();\n"
          "          // Convert the random number to S(0,15) between -0.25 and 0.25\n"
          "          tmp16 = tmp & 0xFFFF;\n" 
          "          tmp16 >>= 3;\n"
          "          // multiply by volume, convert result to S(0,15)\n"
          "          tmp = (volume * (int32_t)tmp16)>>16;\n"
          "          // store in buffer\n"
          "          buffer[i] = tmp;\n"
          "        }\n"
	  "      ks_chan[ks_channel].timer[index]=80*array_size[index];\n"
          "    }\n"
          "}\n\n");
  
  fprintf(ksc,
          "#ifdef GET_KS_MIN_MAX\n"
          "int ks_min_smpl = INT_MAX;\n"
          "int ks_max_smpl = INT_MIN;\n"
          "#endif\n\n");

  fprintf(ksc,"void ks_fill_buffer(int ks_channel, int16_t *data)\n"
          "{\n"
          "  int32_t tmp_buffer[AUDIO_BUFFER_LENGTH];\n"
          "  int i,j;\n"
          "  int32_t temp;\n"
          "  int nextpos;\n"
          "  int16_t *buffer;\n"
          "  int32_t value;\n"
          "  int32_t fact = 0xFEFE;  // a U(0,16) that is slightly less than 1.0\n"
          "  int32_t cposition;\n"
          "  int32_t csize;\n"
          "  // clear the temporary buffer\n"
          "  memset(tmp_buffer,0,sizeof(int32_t) * AUDIO_BUFFER_LENGTH);\n"
          "  // generate some audio samples for each active string,\n"
          "  // adding them together in tmp_buffer.\n"
          "  for(j = 0; j <= LAST_MIDI-FIRST_MIDI; j++)\n"
          "    {\n"
          "      if(ks_chan[ks_channel].timer[j] > 0)\n"
          "	 {\n"
          "	   cposition = ks_chan[ks_channel].position[j];\n"
          "	   csize = array_size[j];\n"
	  "        buffer = get_bufptr(ks_channel,j);\n"
          "	   for(i=0; i < AUDIO_BUFFER_LENGTH; i++)\n"
          "	     {\n"
          "	        // update each active string and add its output to the sum\n"
          " 	        nextpos = (cposition+1) %% csize;\n"
          " 	        // calculate value as an S(0,31) \n"
          " 	        value=(fact*((int32_t)buffer[cposition] + \n"
          "                          (int32_t)buffer[nextpos])+1)>>1;\n"
          " 	        value >>= 16;  // convert to S(0,15)\n"
          " 	        buffer[cposition] = value;\n"
          " 	        tmp_buffer[i] += value;\n"
          " 	        cposition = nextpos;\n"
	  "#ifdef GET_KS_MIN_MAX\n"
	  "             ks_min_smpl = (value < ks_min_smpl)?value:ks_min_smpl;\n"
	  "             ks_max_smpl = (value > ks_max_smpl)?value:ks_max_smpl;\n"
	  "#endif\n"
          " 	    }\n"
          " 	  ks_chan[ks_channel].position[j] = cposition;\n"
          " 	  ks_chan[ks_channel].timer[j]--;\n"
          " 	}\n"
          "    } \n"
          "  // copy the results from tmp_buffer to data.\n"
          "  for(i=0; i < AUDIO_BUFFER_LENGTH; i++)\n"
          "    {\n"
          "      temp = tmp_buffer[i];\n"
	  "      // saturate temp to the number of bits specified\n"
	  "      // temp = temp > ((1<<BIT_DEPTH)-1)?(1<<BIT_DEPTH)-1:temp;\n"
	  "      // temp = temp < -(1<<BIT_DEPTH)?-(1<<BIT_DEPTH):temp;\n"
	  "      // store the sample in output buffer\n"
          "      data[i] = (uint16_t)temp;\n"
          "    }\n"
          "}\n");
	  

  fclose(ksc);

  ksh = fopen("ks.h","w");
  fprintf(ksh,"#ifndef KS_H\n"
          "#define KS_H\n\n"
          "#include <stdint.h>\n\n"
          "/* If we change the sample rate, we have to re-calculate the\n"
          "   lengths of all of the \"string\" buffers in ks.c */\n"
          "#define SAMPLE_RATE %d\n"
          "#define BIT_DEPTH %d\n",rate,bit_depth);
  fprintf(ksh,"#define AUDIO_BUFFER_LENGTH %d\n\n",64);
  fprintf(ksh,"/* This function simulates plucking one string */\n"
          "void ks_string_pluck(int ks_channel, int16_t midi_num, int16_t volume);\n\n"
          "/* This function fills a buffer with audio samples. */\n"
          "void ks_fill_buffer(int ks_channel, int16_t *data);\n\n"
          "#endif\n"
          );

  fclose(ksh);
  
  return 0;
}

