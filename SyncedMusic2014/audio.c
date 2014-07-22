#include <Windows.h>
#include <stdio.h>
#include "audio.h"

#define PA_SAMPLE_TYPE paFloat32

PaStream* setupStream(double sampleRate, PaDeviceIndex inputDevice, PaDeviceIndex outputDevice)
{
	PaStreamParameters paInputParameters, paOutputParameters;
	PaStream *paStream = NULL;
	PaError paError;

	paError = Pa_Initialize();
	if (paError != paNoError) {
		printf("Pa_Initialize failed with error: %d\n", paError);
		return NULL;
	}
	
	paInputParameters.device = inputDevice;
	paInputParameters.channelCount = Pa_GetDeviceInfo(inputDevice)->maxInputChannels;
	paInputParameters.sampleFormat = PA_SAMPLE_TYPE;
	paInputParameters.suggestedLatency = Pa_GetDeviceInfo(inputDevice)->defaultLowInputLatency;
	paInputParameters.hostApiSpecificStreamInfo = NULL;

	paOutputParameters.device = outputDevice;
	paOutputParameters.channelCount = Pa_GetDeviceInfo(outputDevice)->maxOutputChannels;
	paOutputParameters.sampleFormat = PA_SAMPLE_TYPE;
	paOutputParameters.suggestedLatency = Pa_GetDeviceInfo(outputDevice)->defaultHighOutputLatency;
	paOutputParameters.hostApiSpecificStreamInfo = NULL;

	paError = Pa_OpenStream(
		&paStream,
		&paInputParameters,
		&paOutputParameters,
		sampleRate,
		FRAMES_PER_BUFFER,
		paClipOff,   
		NULL, 
		NULL);

	if (paError != paNoError) {
		printf("Pa_OpenStream failed with error: %d\n", paError);
		return NULL;
	}

	paError = Pa_StartStream(paStream);
	if (paError != paNoError) {
		printf("Pa_OpenStream failed with error: %d\n", paError);
		return NULL;
	}
}

void printDeviceInfo()
{
	Pa_Initialize();
	puts("=== PORTAUDIO DEVICES ===");
	printf("default input device: %d\n", Pa_GetDefaultInputDevice());
	printf("default output device: %d\n", Pa_GetDefaultOutputDevice());
	puts("");

	for (PaDeviceIndex deviceIndex = 0; deviceIndex < Pa_GetDeviceCount(); deviceIndex++) {
		PaDeviceInfo* info = Pa_GetDeviceInfo(deviceIndex); // must not free this memory
		if (info != NULL) {
			printf("device index: %d\n", deviceIndex);
			printf("host api index: %d\n", info->hostApi);
			printf("name: %s\n", info->name);
			printf("default sample rate: %.0f Hz\n", info->defaultSampleRate);
			if (info->maxInputChannels > 0) {
				printf("maximum number of input channels: %d\n", info->maxInputChannels);
				printf("default high input latency (non-interactive): %.1f ms\n", info->defaultHighInputLatency*1000.0);
				printf("default low input latency (interactive): %.1f ms\n", info->defaultLowInputLatency*1000.0);
			}
			else {
				puts("no input");
			}
			if (info->maxOutputChannels > 0) {
				printf("maximum number of output channels: %d\n", info->maxOutputChannels);
				printf("default high output latency (non-interactive): %.1f ms\n", info->defaultHighOutputLatency*1000.0);
				printf("default low output latency (interactive): %.1f ms\n", info->defaultLowOutputLatency*1000.0);
			}
			else {
				puts("no output");
			}
			puts("");
		}
	}
	puts("=========================");
	Pa_Terminate();
}

