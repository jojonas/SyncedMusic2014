#include "client.h"

#include <stdio.h>
#include <signal.h>

#include "network.h"
#include "server.h"

#define BUFFERSIZE 1024

volatile BOOL terminateClient;

void ctrlCClientHandler(int signal) {
	if (signal == SIGINT) {
		terminateClient = TRUE;
	}
}

int clientMain(int argc, char** argv)
{
	puts("Initiated client mode.");

	SOCKET clientSocket = setupConnection(argv[1], SERVER_PORT);
	if (clientSocket == INVALID_SOCKET) {
		printf("could not connect to server\n");
		return 1;
	}

	TimerState* timer = createTimer();

	char* buffer = malloc(BUFFERSIZE*sizeof(char));
	unsigned int dataEndIndex = 0;

	signal(SIGINT, ctrlCClientHandler);

	while (!terminateClient) {
		const int bytesReceived = recv(clientSocket, buffer + dataEndIndex, BUFFERSIZE - dataEndIndex, 0);
		dataEndIndex += bytesReceived;
		unsigned int size;
		while ((size = *(unsigned int*)buffer) <= dataEndIndex) {
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
				puts("Traceback (most recent call last):\n  File \"<stdin>\", line 1, in < module >\nNotImplementedError");
				break;
			default:
				puts("oh fuck. gg.");
			}

			memmove(buffer, buffer + size, dataEndIndex - size);
			dataEndIndex -= size;
		}
		printf("client time: %f\n", getTime(timer));
	}

	shutdown(clientSocket, SD_SEND);

	int bytesReceived;
	do {
		bytesReceived = recv(clientSocket, buffer, BUFFERSIZE, 0);
		if (bytesReceived == SOCKET_ERROR) {
			puts("graceful socket shutdown failed");
			break;
		}
	} while (bytesReceived != 0);
	closesocket(clientSocket);
	free(buffer);

	return 0;
}
