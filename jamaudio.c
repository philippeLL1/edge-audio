#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <math.h>
#include <features.h>
#include <alsa/asoundlib.h>
#include "include/alsa-config.h"
#include "include/alsa-utils.h"
#include "include/jamaudio.h"

int play = 0;
double note = 440.;

double midiToHz(int note) {
    double power = (note - 69) / (double)12;
    return 440 * pow(2, power);
}
// void* generate_sine(void *args)
// {
//   sine_gen_t *context = (sine_gen_t *) args;
//   const snd_pcm_channel_area_t *areas = context->areas;
//   snd_pcm_uframes_t offset = context->offset;
//   int count    = context->count;
// 	double phase = *context->phase;
// 	double step  = (MAX_PHASE) * (context->freq) / (double)RATE;
//
// 	unsigned char *samples[CHANNELS];
// 	int steps[CHANNELS];
// 	unsigned int chn;
// 	int format_bits = snd_pcm_format_width(FORMAT);
// 	unsigned int maxval = (1 << (format_bits - 1)) - 1;
// 	int bps = format_bits / 8;  /* bytes per sample */
//
// 	/* verify and prepare the contents of areas */
// 	for (chn = 0; chn < CHANNELS; chn++) {
// 		if ((areas[chn].first % 8) != 0) {
// 			printf("areas[%u].first == %u, aborting...\n", chn, areas[chn].first);
// 			exit(EXIT_FAILURE);
//     }
//
// 		samples[chn] = (((unsigned char *)areas[chn].addr) + (areas[chn].first / 8));
//
// 		if ((areas[chn].step % 16) != 0) {
// 			printf("areas[%u].step == %u, aborting...\n", chn, areas[chn].step);
// 			exit(EXIT_FAILURE);
// 		}
// 		steps[chn] = areas[chn].step / 8;
// 		samples[chn] += offset * steps[chn];
// 	}
// 	/* fill the channel areas */
// 	while (count-- > 0) {
//
// 		int result = sin(phase) * maxval;
// 		for (chn = 0; chn < CHANNELS; chn++) {
//       for (int i = 0; i < bps; i++) {
        // *(samples[chn] + i) = (result >> i * 8) & 0xff;
//       }
// 			samples[chn] += steps[chn];
// 		}
// 		phase += step;
// 		if (phase >= MAX_PHASE)
// 			phase -= MAX_PHASE;
// 	}
// 	*context->phase = phase;
//   return NULL;
// }

/*
 *   Transfer method - write only
 */

//
// int write_loop(SoundConfig *config, SoundData *data)
// {
// 	double phase = 0;
// 	int err, cptr;
// 	signed short *ptr;
//   snd_pcm_uframes_t offset = 0;
//
//   sine_gen_t waveInfo = { data->areas, offset, config->periodSize, &phase, data->freq };
//   pthread_t sineGenId;
//
//   for (int i = 0; i < 2; i++) {
//     generate_sine((void *) &waveInfo);
//     ptr = data->samples;
//     cptr = (config->periodSize);
//     while (cptr > 0) {
//       err = snd_pcm_writei(config->pcmHandle, ptr, cptr);
//       if (err == -EAGAIN)
//         continue;
//       if (err < 0) {
//         if (xrun_recovery(config->pcmHandle, err) < 0) {
//           printf("Write error: %s\n", snd_strerror(err));
//           exit(EXIT_FAILURE);
//         }
//         //break;	/* skip one period */
//       }
//       ptr += err * CHANNELS;
//       cptr -= err;
//     }
//   }
//   return 0;
// }
//
// void *noteOutput(void *args) {
//
//   SoundData *data = (SoundData *) args;
//   SoundConfig *config = data->config;
//
//   for (;;) {
//     data->freq = note;
//     if (play == 1) {
//       write_loop(config, data);
//       // usleep(200);
//     }
//   }
// }
//
void midiCallback(snd_seq_t *sequencerHandle) {

  snd_seq_event_t *event;

  do {
    snd_seq_event_input(sequencerHandle, &event);
    
    switch (event->type) {
      case SND_SEQ_EVENT_NOTEON:
          note = midiToHz(event->data.note.note);
          play = 1;
          break;
      case SND_SEQ_EVENT_NOTEOFF:
          play = 0;
          break;
    }

    snd_seq_free_event(event);

  } while (snd_seq_event_input_pending(sequencerHandle, 0) > 0);
}

void *midiLoop(void *input) {
    midi_thread_t *inputData = (midi_thread_t *) input;
    for (;;) {
        if ((poll(inputData->pollfdArray, inputData->countSequencerFileDesc, 500)) > 0) {
            if (inputData->pollfdArray[0].revents > 0) 
                midiCallback(inputData->sequencerHandle);
        }
    }
}

snd_seq_t *initSequencer(int *sequencerPort) {

    snd_seq_t *sequencerHandle;
    int portId;

    if (snd_seq_open(&sequencerHandle, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        fprintf(stderr, "Could not open ALSA Sequencer.\n");
        exit(1);
    }
    snd_seq_set_client_name(sequencerHandle, "JAMAudio");
    if ((*sequencerPort = (snd_seq_create_simple_port(sequencerHandle, "JAMAudio",
        SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_APPLICATION))) < 0) {
        fprintf(stderr, "Could not create sequencer port.\n");
        exit(1);
    }
    return(sequencerHandle);
}

int main(int argc, char **argv) {
    int midiAddress, midiPort; 
  int countSequencerFileDesc, sequencerPort;
  int error;
  unsigned short midiEvent;

  /* Parse input arguments */
  if (argc != 3) {
    printf("Usage: jamaudio [ADDRESS] [PORT]\n");
    exit(1);
  } else {
    midiAddress = atoi(argv[1]);
    midiPort = atoi(argv[2]);
  }

  /* Start PCM Interface */
  SoundData data = {};
  SoundConfig config = {};
  data.config = &config;
  initPCMPlayback(&data);

  /* Initialize Sequencer and File Descriptors for Polling */ 
  snd_seq_t *sequencerHandle;
  struct pollfd *pollfdArray;
  sequencerHandle = initSequencer(&sequencerPort);

  /* Connect MIDI Keyboard to ALSA Sequencer */ 
  if ((error = snd_seq_connect_from(sequencerHandle, sequencerPort, midiAddress, midiPort)) < 0) {
    fprintf(stderr, "Could not connect to the input MIDI device: %s\n", snd_strerror(error));
    exit(1);
  }

  /* Start polling for MIDI events */
  countSequencerFileDesc = snd_seq_poll_descriptors_count(sequencerHandle, POLLIN);
  pollfdArray = (struct pollfd *) alloca(sizeof(struct pollfd) * (countSequencerFileDesc));
  snd_seq_poll_descriptors(sequencerHandle, pollfdArray, countSequencerFileDesc, POLLIN);

  midi_thread_t midiArgs = { pollfdArray, countSequencerFileDesc, sequencerHandle };
  pthread_t midiId;
  
  pthread_create(&midiId, NULL, (void *)midiLoop, &midiArgs);

  pthread_t noteId;
  //pthread_create(&noteId, NULL, (void *)noteOutput, (void *) &data);

  pthread_join(midiId, NULL);
  pthread_join(noteId, NULL);

	free(data.areas);
	free(data.samples);
  snd_seq_close(sequencerHandle);
	snd_pcm_close((data.config)->pcmHandle);
  return 0;
}

