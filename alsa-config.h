#ifndef ALSA_CONFIG_H
#define ALSA_CONFIG_H

#include <alsa/asoundlib.h>

#define RATE          (44100)
#define CHANNELS      (2)
#define FORMAT        (SND_PCM_FORMAT_S16_LE)
#define BUFFER_TIME   (800000)
#define PERIOD_TIME   (100000)
#define PERIOD_EVENTS (1)
#define VERBOSE       (0)
#define RESAMPLE      (1)
#define ACCESS        (SND_PCM_ACCESS_RW_INTERLEAVED)
#define STARTING_FREQ (440.)

#endif
