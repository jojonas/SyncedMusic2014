#ifndef NETWORK_H
#define NETWORK_H

//#include <Windows.h>
//#include <winsock.h>
#include <WS2tcpip.h>

#include "audio.h"
#include "time.h"

#define PACKETTYPE_TIMESTAMP 1
#define PACKETTYPE_SOUND 2

typedef float sample_t;
typedef int PacketType;

#pragma pack(push,1) // disable alignment locally
typedef struct {
	unsigned int size;
	PacketType type;
	timer_t playTime;
	sample_t samples[AUDIO_BUFFER_SIZE];
} SoundPacket;

typedef struct {
	unsigned int size;
	PacketType type;
	timer_t time;
} TimestampPacket;
#pragma pack(pop) // end disable alignment

SOCKET setupListeningSocket(const unsigned short port);
SOCKET setupConnection(const char* host, const int port);

#endif // NETWORK_H
