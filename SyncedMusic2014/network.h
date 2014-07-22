#ifndef NETWORK_H
#define NETWORK_H

//#include <Windows.h>
//#include <winsock.h>
#include <WS2tcpip.h>

#include "time.h"

#define SAMPLES_PER_PACKET 12

typedef float sample_t;

typedef struct {
	timer_t playTime;
	sample_t samples[SAMPLES_PER_PACKET];
} SoundPacket;

typedef struct {
	timer_t time;
} TimestampPacket;


SOCKET setupListeningSocket(const unsigned short port);

#endif // NETWORK_H
