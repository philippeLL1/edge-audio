#ifndef WAVEFORMS_H
#define WAVEFORMS_H

#include <alsa/asoundlib.h>


typedef struct SoundConfig {
    snd_pcm_t           *pcmHandle;
    snd_pcm_sframes_t   bufferSize;
    snd_pcm_sframes_t   periodSize;
	snd_pcm_hw_params_t *hwParams;
	snd_pcm_sw_params_t *swParams;
    char                *outputDevice;

} SoundConfig;

typedef struct SoundData {
    signed short *samples;
    snd_pcm_channel_area_t *areas;
    double freq;
    SoundConfig *config;
} SoundData;

typedef struct OscillatorInput {
    snd_pcm_sframes_t num_frames;
    unsigned int max_val;
    double frequency, gain, sample_rate;
    int velocity;
} OscillatorInput;

typedef struct SoundLoopArgs {
    OscillatorInput wave_data;
    SoundData       sound_data;
    int             sweep_on;
    int             repeats;
} SoundLoopArgs;

#endif
