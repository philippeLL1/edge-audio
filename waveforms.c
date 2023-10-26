#include "include/waveforms.h"
#include "include/alsa-utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <alsa/asoundlib.h>
#include <sys/wait.h>

void print_sampling_info() {

 	int format_bits = snd_pcm_format_width(SND_PCM_FORMAT_S16);
 	unsigned int maxval = (1 << (format_bits - 1)) - 1;
 	int bps = format_bits / 8;  

    printf("Format bits: %d \n Max Value: %u \n Bytes per Sample: %d \n", format_bits, maxval, bps);
}

// Writes a sine wave to the input buffer in PCM interleaved format
void sine_oscillator(unsigned char *buffer, OscillatorInput params, double* input_phase) {
    double phase, phase_step, period_length;
    int output;
    char lsb, msb;

    phase = *input_phase;
    period_length = 2. * M_PI;
    phase_step = M_PI * params.frequency / (params.sample_rate / 2.);

    for (int i = 0; i < params.num_frames * 2; ++i) {

        // apply digital signal processing on amplitude value
        output = params.max_val * (params.gain * sin(phase));
        
        // Extract the MSB and LSB to interleave them according to our format
        lsb = output & 0xff;
        msb = output >> 8 & 0xff;

        buffer[2 * i]     = lsb;
        buffer[2 * i + 1] = msb;

        phase += phase_step;
        if (phase >= period_length)
            phase -= period_length;
    }
    *input_phase = phase;
}


double pitchBend(int midiNote, double pitch) {

    double low, correct, high, frequency;

    low     = 8.176 * exp((double)(midiNote-2)*log(2.0)/12.0);
    correct = 8.176 * exp((double)(midiNote)*log(2.0)/12.0);
    high    = 8.176 * exp((double)(midiNote+2)*log(2.0)/12.0);

    // adjust frequency based on pitch shift value
    if (pitch > 0) {
        frequency = correct + (high - low) * pitch;
    } else {
        frequency = correct + (correct - low) * pitch;
    }
    
    return frequency;
}

void* sound_loop(void *args){
    SoundLoopArgs *input_args = (SoundLoopArgs*)args;
    OscillatorInput wave_data; 
    SoundData data;
    double phase, full_gain, gain_step;
    unsigned char *result, *frames;
    int count, err, sweep;

    phase     = 0.;
    sweep     = input_args->sweep_on;
    wave_data = input_args->wave_data;
    data      = input_args->sound_data;
    full_gain = wave_data.gain;

    wave_data.gain = 0;
    gain_step = full_gain / 10.;

    result = malloc(sizeof(unsigned char) * data.config->bufferSize);

    for (int i = 0; i < input_args->repeats; ++i) {

        //Fade in/out
        if (i < input_args->repeats - (input_args->repeats - 5)) {
            wave_data.gain += gain_step;
        } else if (i >= input_args->repeats - 5) {
            wave_data.gain -= gain_step;
        }


        sine_oscillator(result, wave_data, &phase);
        frames = result;
        count  = data.config->periodSize;
        while (count > 0) {
            err = snd_pcm_writei((data.config)->pcmHandle, frames, count);
            if (err == -EAGAIN) {
                continue;
            }
            if (err < 0) {
                if (xrun_recovery(data.config->pcmHandle, err) < 0) {
                    fprintf(stderr, "Write error: %s\n", snd_strerror(err));
                    exit(EXIT_FAILURE);
                }
                break;	
            }
            frames += err;
            count -= err;
        }
        // if (sweep != 0) {
        //     if (wave_data.frequency > 600.) {
        //         sweep = -1;
        //     } else if (wave_data.frequency < 200) {
        //         sweep = 1;
        //     }
        //     wave_data.frequency += sweep * 60;
        // }
    }
    free(result);
    return NULL;
}

void* wav_loop(void *args){
    SoundLoopArgs *input_args = (SoundLoopArgs*)args;
    SoundData data;
    double full_gain, gain_step;
    unsigned char *result, *frames;
    int count, err, sweep;

    data = input_args->sound_data;

    for (int i = 0; i < input_args->repeats; ++i) {


        count  = data.config->periodSize;
        while (count > 0) {
            err = snd_pcm_writei((data.config)->pcmHandle, frames, count);
            if (err == -EAGAIN) {
                continue;
            }
            if (err < 0) {
                if (xrun_recovery(data.config->pcmHandle, err) < 0) {
                    fprintf(stderr, "Write error: %s\n", snd_strerror(err));
                    exit(EXIT_FAILURE);
                }
                break;	
            }
            frames += err;
            count -= err;
        }
        // if (sweep != 0) {
        //     if (wave_data.frequency > 600.) {
        //         sweep = -1;
        //     } else if (wave_data.frequency < 200) {
        //         sweep = 1;
        //     }
        //     wave_data.frequency += sweep * 60;
        // }
    }
    free(result);
    return NULL;
}


int main(int argc, char **argv) {
    OscillatorInput wave_data, wave_data2, wave_data3;
    unsigned char *result, *result2;
    unsigned char *frames;
    int err, count, format_bits, bytes_per_sample, sweep;
    SoundData data, data2, data3;
    SoundConfig config, config2, config3;
    unsigned int max_val;
    
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <frequency_1> <frequency_2> <wav_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

 	format_bits = snd_pcm_format_width(SND_PCM_FORMAT_S16);
 	max_val = (1 << (format_bits - 1)) - 1;
 	bytes_per_sample = format_bits / 8;  

    data.config = &config;
    initPCMPlayback(&data);

    data2.config = &config2;
    initPCMPlayback(&data2);
 
    data3.config = &config3;
    initPCMPlayback(&data3);   

    wave_data = (OscillatorInput){.num_frames=data.config->periodSize, .frequency=atof(argv[1]), .gain=0.3, .sample_rate=44100.0, .velocity=80, .max_val=max_val};


    wave_data2 = (OscillatorInput){.num_frames=data2.config->periodSize, .frequency=atof(argv[2]), .gain=0.3, .sample_rate=44100.0, .velocity=80, .max_val=max_val};

    wave_data3 = (OscillatorInput){.num_frames=data2.config->periodSize, .frequency=atof(argv[3]), .gain=0.3, .sample_rate=44100.0, .velocity=80, .max_val=max_val};

    SoundLoopArgs args1 = { wave_data, data, 0, 100};
    SoundLoopArgs args2 = { wave_data2, data2, 0, 100};
    SoundLoopArgs args3 = { wave_data3, data3, 0, 100};

    pthread_t id1, id2, id3;
    pthread_create(&id1, NULL, (void*)sound_loop, (void*)&args1);
    pthread_create(&id2, NULL, (void*)sound_loop, (void*)&args2);
    pthread_create(&id3, NULL, (void*)sound_loop, (void*)&args3);
    pthread_join(id1, NULL);
    pthread_join(id2, NULL);
    pthread_join(id3, NULL);

	free(data2.areas);
	free(data2.samples);
	free(data.areas);
	free(data.samples);
	snd_pcm_close((data.config)->pcmHandle);
	snd_pcm_close((data2.config)->pcmHandle);
    return 0;
}

