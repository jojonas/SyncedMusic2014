#include "client.h"

#include <assert.h>
#include <stdio.h>
#include <signal.h>

#include "network.h"
#include "server.h"

#define NETWORK_BUFFER_SIZE 1024*1024

volatile BOOL terminateClient;

void ctrlCClientHandler(int signal) {
	if (signal == SIGINT) {
		terminateClient = TRUE;
	}
}

int clientMain(int argc, char** argv)
{
	assert(AUDIO_BUFFER_SIZE <= NETWORK_BUFFER_SIZE);

	puts("info:");
	printAudioDeviceList();

	puts("Initiated client mode.");

	char* buffer = malloc(NETWORK_BUFFER_SIZE*sizeof(char));
	if (!buffer) {
		printf("could not allocate %d bytes for network buffer\n", NETWORK_BUFFER_SIZE);
		return 1;
	}

	SOCKET clientSocket = setupConnection(argv[1], SERVER_PORT);
	if (clientSocket == INVALID_SOCKET) {
		printf("could not connect to server\n");
		free(buffer);
		return 1;
	}

	TimerState* timer = createTimer();

	unsigned int dataEndIndex = 0;

	Pa_Initialize();
	const PaDeviceIndex defaultOutputDevice = Pa_GetDefaultOutputDevice();
	PaStream* paStream = setupStream(-1, defaultOutputDevice);

	signal(SIGINT, ctrlCClientHandler);
	while (!terminateClient) {
		const int bytesReceived = recv(clientSocket, buffer + dataEndIndex, NETWORK_BUFFER_SIZE - dataEndIndex, 0);

		if (bytesReceived == SOCKET_ERROR) {
			const int error = WSAGetLastError();
			printf("recv failed with error: %d\n", error);
			if (error == WSAECONNRESET) {
				puts("connection reset by peer, quiting");
				break;
			}
			else {
				continue;
			}
		}

		dataEndIndex += bytesReceived;

		unsigned int size;
		while (dataEndIndex > 0 && (size = *(unsigned int*)buffer) <= dataEndIndex) {
			printf("client packet size: %d\n", size);
			// paket ganz!
			const PacketType type = *(PacketType*)(buffer + sizeof(unsigned int));
			switch (type) {
			case PACKETTYPE_TIMESTAMP: 
				{
					TimestampPacket* packet = (TimestampPacket*)buffer;
					updateTimer(timer, packet->time);
				}
				break;
			case PACKETTYPE_SOUND:
				{
					SoundPacket* packet = (SoundPacket*)buffer;
					PaError error = Pa_WriteStream(paStream, packet->samples, FRAMES_PER_PACKET);
					if (error != paNoError) {
						printf("Pa_WriteStream failed with error: %d\n", error);
					}
				}
				break;
			default:
				printf("Invalid packet type: %d\n", type);
				break;
			}

			memmove(buffer, buffer + size, dataEndIndex - size);
			dataEndIndex -= size;
		}
	}

	shutdown(clientSocket, SD_SEND);

	int bytesReceived;
	do {
		bytesReceived = recv(clientSocket, buffer, NETWORK_BUFFER_SIZE, 0);
		if (bytesReceived == SOCKET_ERROR) {
			puts("graceful socket shutdown failed");
			break;
		}
	} while (bytesReceived != 0);
	closesocket(clientSocket);
	

	Pa_AbortStream(paStream);
	Pa_CloseStream(paStream);
	Pa_Terminate();

	free(timer);

	free(buffer);

	return 0;
}
