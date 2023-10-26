
waveform: alsa-utils.c include/alsa-utils.h include/alsa-config.h include/waveforms.h waveforms.c 
	gcc -o waveform waveforms.c alsa-utils.c -lm -lpthread -lasound
