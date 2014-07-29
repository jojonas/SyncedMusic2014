#ifndef AUDIO_H
#define AUDIO_H

#include "portaudio.h"


#define SAMPLE_TYPE paFloat32
#define NUM_CHANNELS 2
#define SAMPLE_RATE 44100

#define PLAY_DELAY 1.0

#define FRAMES_PER_PACKET (int)(0.1*SAMPLE_RATE)
#define BYTES_PER_SAMPLE 4

#define AUDIO_BUFFER_SIZE (BYTES_PER_SAMPLE*NUM_CHANNELS*FRAMES_PER_PACKET)

PaStream* setupStream(PaDeviceIndex inputDevice, PaDeviceIndex outputDevice);
void printAudioDeviceList();



#endif // AUDIO_H