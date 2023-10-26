/*
 *  This small demo sends a simple sinusoidal wave to your speakers.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <alsa/asoundlib.h>
#include <sys/time.h>
#include <math.h>
#include <pthread.h>
#include "include/alsa-config.h"
#include "include/alsa-utils.h"

 
static snd_output_t *output = NULL;
static unsigned int bufferTime  = BUFFER_TIME;
static unsigned int periodTime  = PERIOD_TIME;


void printInfo(SoundConfig *config) {

	printf("Playback device is %s\n", config->outputDevice);
    printf("Stream parameters are %uHz, %s, %u channels\n", 
        RATE, snd_pcm_format_name(FORMAT), CHANNELS);
}

void initPCMPlayback(SoundData *data) {

  static snd_pcm_t *playbackHandle;
  snd_pcm_hw_params_t *hwParams;
  snd_pcm_sw_params_t *swParams;
  snd_pcm_sframes_t bufferSize;
  snd_pcm_sframes_t periodSize;
  snd_pcm_channel_area_t *areas;
  signed short *samples;
  int error;
  
  snd_pcm_hw_params_alloca(&hwParams);
  snd_pcm_sw_params_alloca(&swParams);

  if ((error = snd_output_stdio_attach(&output, stdout, 0)) < 0) {
    fprintf(stderr, "Could not attach to stdio: %s\n", snd_strerror(error));
    exit(EXIT_FAILURE);
  }
  if ((error = snd_pcm_open(&playbackHandle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
    fprintf(stderr, "Could not open PCM stream: %s\n", snd_strerror(error));
    exit(EXIT_FAILURE);
  }
  
  if ((error = set_hwparams(playbackHandle, hwParams, &bufferSize, &periodSize)) < 0) {
    fprintf(stderr, "Could not initialize hardware parameters: %s\n", snd_strerror(error));
    exit(EXIT_FAILURE);
  }
  if ((error = set_swparams(playbackHandle, swParams, &bufferSize, &periodSize)) < 0) {
    fprintf(stderr, "Could not initialize software parameters: %s\n", snd_strerror(error));
    exit(EXIT_FAILURE);
  }

  if (VERBOSE != 0)
    snd_pcm_dump(playbackHandle, output);
  
  // Samples & Areas represent our digital buffer containing sound information
  samples = malloc(( periodSize * CHANNELS * snd_pcm_format_physical_width(FORMAT)) / 8);
  if (samples == NULL) {
    fprintf(stderr, "Could not allocate memory to samples array \n");
    exit(EXIT_FAILURE);
  }
  areas = calloc(CHANNELS, sizeof(snd_pcm_channel_area_t));
  if (areas == NULL) {
    fprintf(stderr, "Could not alocate memory to PCM channel area \n");
    exit(EXIT_FAILURE);
  }

  for (unsigned int chn = 0; chn < CHANNELS; chn++) {
    areas[chn].addr = samples;
    areas[chn].first = chn * snd_pcm_format_physical_width(FORMAT);
    areas[chn].step = CHANNELS * snd_pcm_format_physical_width(FORMAT);
  }

  SoundConfig *config = data->config;

  config->hwParams = hwParams;
  config->swParams = swParams;
  config->pcmHandle = playbackHandle;
  config->periodSize = periodSize;
  config->bufferSize = bufferSize;
  config->outputDevice = "default";

  data->areas = areas;
  data->samples = samples;
  data->freq = STARTING_FREQ;
  data->config = config;

}

int set_hwparams(snd_pcm_t *handle,
			snd_pcm_hw_params_t *params,
      snd_pcm_sframes_t* buffer_size, 
      snd_pcm_sframes_t* period_size)
{
	unsigned int rate = RATE;
	snd_pcm_uframes_t size;
	int err, dir;

	/* choose all parameters */
	err = snd_pcm_hw_params_any(handle, params);
	if (err < 0) {
		printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
		return err;
	}
	/* set hardware resampling */
	err = snd_pcm_hw_params_set_rate_resample(handle, params, RESAMPLE);
	if (err < 0) {
		printf("Resampling setup failed for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* set the interleaved read/write format */
	err = snd_pcm_hw_params_set_access(handle, params, ACCESS);
	if (err < 0) {
		printf("Access type not available for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* set the sample format */
	err = snd_pcm_hw_params_set_format(handle, params, FORMAT);
	if (err < 0) {
		printf("Sample format not available for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* set the count of channels */
	err = snd_pcm_hw_params_set_channels(handle, params, CHANNELS);
	if (err < 0) {
		printf("Channels count (%u) not available for playbacks: %s\n", CHANNELS, snd_strerror(err));
		return err;
	}
	/* set the stream rate */
	err = snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0);
	if (err < 0) {
		printf("Rate %uHz not available for playback: %s\n", RATE, snd_strerror(err));
		return err;
	}
	if (rate != RATE) {
		printf("Rate doesn't match (requested %uHz, got %iHz)\n", RATE, err);
		return -EINVAL;
	}
	/* set the buffer time */
	err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &bufferTime , &dir);
	if (err < 0) {
		printf("Unable to set buffer time %u for playback: %s\n", bufferTime, snd_strerror(err));
		return err;
	}
	err = snd_pcm_hw_params_get_buffer_size(params, &size);
	if (err < 0) {
		printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
		return err;
	}
  // Resize buffer to nearest possible size
	*buffer_size = size;


	/* set the period time */
	err = snd_pcm_hw_params_set_period_time_near(handle, params, &periodTime, &dir);
	if (err < 0) {
		printf("Unable to set period time %u for playback: %s\n", periodTime, snd_strerror(err));
		return err;
	}
	err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
	if (err < 0) {
		printf("Unable to get period size for playback: %s\n", snd_strerror(err));
		return err;
	}
  // Resize period to closest available size
	*period_size = size;

	/* write the parameters to device */
	err = snd_pcm_hw_params(handle, params);
	if (err < 0) {
		printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
		return err;
	}
	return 0;
}

int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams, snd_pcm_sframes_t *buffer_size, snd_pcm_sframes_t *period_size)
{
	int err;

	/* get the current swparams */
	err = snd_pcm_sw_params_current(handle, swparams);
	if (err < 0) {
		printf("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* start the transfer when the buffer is almost full: */
	/* (buffer_size / avail_min) * avail_min */
	err = snd_pcm_sw_params_set_start_threshold(handle, swparams, (*buffer_size / *period_size) * *period_size);
	if (err < 0) {
		printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* allow the transfer when at least period_size samples can be processed */
	/* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
	err = snd_pcm_sw_params_set_avail_min(handle, swparams, PERIOD_EVENTS ? *buffer_size : *period_size);
	if (err < 0) {
		printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* enable period events when requested */
	if (PERIOD_EVENTS) {
		err = snd_pcm_sw_params_set_period_event(handle, swparams, 1);
		if (err < 0) {
			printf("Unable to set period event: %s\n", snd_strerror(err));
			return err;
		}
	}
	/* write the parameters to the playback device */
	err = snd_pcm_sw_params(handle, swparams);
	if (err < 0) {
		printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
		return err;
	}
	return 0;
}

int xrun_recovery(snd_pcm_t *handle, int err)
{
	if (VERBOSE)
		printf("Performing stream recovery\n");
	if (err == -EPIPE) {	/* under-run */
		err = snd_pcm_prepare(handle);
		if (err < 0)
			printf("Can't recovery from underrun, snd_pcm_prepare() failed: %s\n", snd_strerror(err));
		return 0;
	} else if (err == -ESTRPIPE) {
		while ((err = snd_pcm_resume(handle)) == -EAGAIN)
			sleep(1);	/* wait until the suspend flag is released */
		if (err < 0) {
			err = snd_pcm_prepare(handle);
			if (err < 0)
				printf("Can't recover from suspend, snd_pcm_prepare() failed: %s\n", snd_strerror(err));
		}
		return 0;
	}
	return err;
}

