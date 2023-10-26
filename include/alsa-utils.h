#ifndef ALSA_UTILS_H
#define ALSA_UTILS_H

#include <alsa/asoundlib.h>
#include "waveforms.h"

void initPCMPlayback(SoundData *data);

int write_loop(SoundConfig *config, SoundData *data);

void* generate_sine(void *args);

int write_loop(SoundConfig *config, SoundData *data);

int set_hwparams(snd_pcm_t *handle,
			snd_pcm_hw_params_t *params,
      snd_pcm_sframes_t* buffer_size, 
      snd_pcm_sframes_t* period_size);

 
int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams, snd_pcm_sframes_t *buffer_size, snd_pcm_sframes_t *period_size);

int xrun_recovery(snd_pcm_t *handle, int err);

#endif
