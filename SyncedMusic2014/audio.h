#ifndef AUDIO_H
#define AUDIO_H

#include "portaudio.h"

PaStream* setupStream(double sampleRate, PaDeviceIndex inputDevice, PaDeviceIndex outputDevice);
void printDeviceInfo();

#endif // AUDIO_H