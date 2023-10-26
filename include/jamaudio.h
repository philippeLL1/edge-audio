#ifndef JAMAUDIO_H
#define JAMAUDIO_H

#include <alsa/asoundlib.h>

typedef struct midi_thread_t {
  struct pollfd *pollfdArray;
  unsigned int  countSequencerFileDesc;
  snd_seq_t     *sequencerHandle;
} midi_thread_t;


typedef struct sine_gen_t {
  const snd_pcm_channel_area_t *areas;
  snd_pcm_uframes_t offset;
  int count;
  double *phase;
  double freq;
  
} sine_gen_t;

double midiToHz(int note);


#endif
