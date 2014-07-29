#include "server.h" 

#include <stdio.h>
#include <signal.h>
#include <time.h>

#include "audio.h"
#include "network.h"
#include "time.h"

#define TIMESTAMP_UPDATE_UPPER 1.5f
#define TIMESTAMP_UPDATE_LOWER 0.5f 

#define TIMESTAMP_BROADCAST_UPPER 1.5f
#define TIMESTAMP_BROADCAST_LOWER 0.5f

volatile BOOL terminateServer;

typedef struct {
	fd_set* clients;
	HANDLE clientsMutex;
	SOCKET serverSocket;
	volatile BOOL terminate;
} ServerNetState;


void broadcast(fd_set* clients, const char* const data, const int length, const int flags) {
	for (unsigned int clientIndex = 0; clientIndex < clients->fd_count; clientIndex++) {
		SOCKET clientSocket = clients->fd_array[clientIndex];
		const int sendResult = send(clientSocket, data, length, flags);
		if (sendResult == SOCKET_ERROR) {
			const int error = WSAGetLastError();
			if (error == WSAECONNABORTED) {
				for (unsigned int moveIndex = clientIndex + 1; moveIndex < clients->fd_count; moveIndex++) {
					clients->fd_array[moveIndex-1] = clients->fd_array[moveIndex];
				}
				clients->fd_count--;
				printf("client disconnected, now %d clients\n", clients->fd_count);
			} else {
				printf("broadcast send failed with error: %d (tried to send %d bytes)\n", WSAGetLastError(), length);
			}
		}
		else if (sendResult < length) {
			printf("broadcast send did not transmit entire packet (%d of %d bytes transmitted)\n", sendResult, length);
		}
	}
}


float randf(const float min, const float max) {
	return min + (max - min)*rand() / RAND_MAX;
}

void ctrlCServerHandler(int signal) {
	if (signal == SIGINT) {
		terminateServer = TRUE;
	}
}


int serverMain(int argc, char** argv)
{
	puts("info:");
	printAudioDeviceList();

	puts("Initiated server mode.");

	terminateServer = FALSE;

	SoundPacket* packet = malloc(sizeof(SoundPacket));
	if (!packet) {
		puts("could not allocate memory for sound packet");
		return 1;
	}

	TimerState* timerState = createTimer();

	SOCKET serverSocket = setupListeningSocket(SERVER_PORT);
	if (serverSocket == INVALID_SOCKET) {
		printf("could not set up listening socket\n");
		return 1;
	}

	fd_set clients;
	clients.fd_count = 0;
	
	timer_t nextTimestampUpdateAt = 0.0f;
	timer_t nextTimestampBroadcastAt = 0.0f;

	Pa_Initialize();
	const PaDeviceIndex defaultInputDevice = Pa_GetDefaultInputDevice();
	PaStream* paStream = setupStream(defaultInputDevice, -1);

	
	signal(SIGINT, ctrlCServerHandler);
	while (!terminateServer) {
		const timer_t now = getHighPrecisionTime();

		printf("now: %f\n", now);

		fd_set readFs;
		readFs.fd_array[0] = serverSocket;
		readFs.fd_count = 1;

		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;

		const int readySocketCount = select(0, &readFs, NULL, NULL, &timeout);
		if (readySocketCount == 1) {
			SOCKET clientSocket = accept(serverSocket, NULL, NULL);
			if (clientSocket == INVALID_SOCKET) {
				if (WSAGetLastError() != WSAEWOULDBLOCK) {
					printf("accept failed: %d\n", WSAGetLastError());
				}
			}
			else {
				clients.fd_array[clients.fd_count] = clientSocket;
				clients.fd_count++;
				printf("client accepted, now %d clients\n", clients.fd_count);
				if (clients.fd_count > FD_SETSIZE) {
					puts("maximum client size reached");
				}
			}
		}


		if (now > nextTimestampUpdateAt) {
			const timer_t lowPrecisionTime = (timer_t)clock() / CLOCKS_PER_SEC;
			updateTimer(timerState, lowPrecisionTime);
			nextTimestampUpdateAt = now + randf(TIMESTAMP_UPDATE_LOWER, TIMESTAMP_UPDATE_UPPER);
		}

		if (now > nextTimestampBroadcastAt) {
			TimestampPacket packet;
			packet.type = PACKETTYPE_TIMESTAMP;
			packet.size = sizeof(packet);
			packet.time = getTime(timerState);
			broadcast(&clients, (const char*)&packet, packet.size, 0);

			nextTimestampBroadcastAt = now + randf(TIMESTAMP_BROADCAST_LOWER, TIMESTAMP_BROADCAST_UPPER);
		}

		packet->type = PACKETTYPE_SOUND;
		packet->playTime = getTime(timerState) + PLAY_DELAY;
		puts("reading stream");
		const PaError error = Pa_ReadStream(paStream, packet->samples, FRAMES_PER_PACKET);
		if (error != paNoError) {
			printf("Pa_ReadStream failed with code: %d\n", error);
		}
		packet->size = sizeof(SoundPacket);
		puts("broadcasting");
		broadcast(&clients, (const char*)packet, packet->size, 0);
		puts("done");
		// -> falls Client disconnected, hole Mutex und entferne aus Liste (das wird Scheiße, weil das ein Array ist... Schieberei?)

		
	}

	Pa_AbortStream(paStream);
	Pa_CloseStream(paStream);
	Pa_Terminate();

	shutdown(serverSocket, SD_BOTH);
	// eigentlich: immer recv an alle Clients anfragen, bis alle mal 0 zurückgegeben haben, siehe http://msdn.microsoft.com/de-de/library/windows/desktop/ms740481(v=vs.85).aspx
	closesocket(serverSocket);

	free(timerState);

	return 0;
}
