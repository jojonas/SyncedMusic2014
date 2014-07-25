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
} ServerThreadState;

void broadcast(const ServerThreadState* const state, const char* const data, const int length, const int flags) {
	DWORD waitResult = WaitForSingleObject(state->clientsMutex, INFINITE);
	unsigned int clientCount = 0;
	if (waitResult != WAIT_OBJECT_0) {
		printf("cannot acquire client mutex for broadcasting: %d\n", waitResult);
		return;
	}
	clientCount = state->clients->fd_count;
	ReleaseMutex(state->clientsMutex);

	for (unsigned int clientIndex = 0; clientIndex < clientCount; clientIndex++) {
		SOCKET clientSocket = state->clients->fd_array[clientIndex];
		const int sendResult = send(clientSocket, data, length, flags);
		if (sendResult == SOCKET_ERROR) {
			// TODO: react on disconnect
			printf("broadcast send failed with error: %d\n", WSAGetLastError());
		}
		else if (sendResult < length) {
			printf("broadcast send did not transmit entire packet (%d of %d bytes transmitted)\n", sendResult, length);
		}
	}
}


float randf(const float min, const float max) {
	return min + (max - min)*rand() / RAND_MAX;
}

DWORD WINAPI serverLoop(void* parameter) {
	ServerThreadState* state = parameter;
	while (!state->terminate) {
		SOCKET clientSocket = accept(state->serverSocket, NULL, NULL);
		if (clientSocket == INVALID_SOCKET) {
			if (WSAGetLastError() != WSAEWOULDBLOCK) {
				printf("accept failed: %d\n", WSAGetLastError());
			}
		}
		else {
			puts("Client accepted");
			DWORD waitResult = WaitForSingleObject(state->clientsMutex, INFINITE);
			if (waitResult == WAIT_OBJECT_0) {
				state->clients->fd_array[state->clients->fd_count] = clientSocket;
				state->clients->fd_count++;
				if (state->clients->fd_count > FD_SETSIZE) {
					puts("maximum client size reached");
				}
				ReleaseMutex(state->clientsMutex);
			}
			else {
				printf("cannot acquire mutex for adding clients, error: %d\n", waitResult);
				return 1;
			}
		}
	}
	return 0;
}

void ctrlCServerHandler(int signal) {
	if (signal == SIGINT) {
		terminateServer = TRUE;
	}
}


int serverMain(int argc, char** argv)
{
	puts("Initiated server mode.");

	char* networkBuffer = malloc(4096 * sizeof(char));

	terminateServer = FALSE;

	TimerState* timerState = createTimer();
	SOCKET serverSocket = setupListeningSocket(SERVER_PORT);
	if (serverSocket == INVALID_SOCKET) {
		printf("could not set up listening socket\n");
		return 1;
	}

	fd_set clients;
	clients.fd_count = 0;

	ServerThreadState serverThreadState;
	serverThreadState.clients = &clients;
	serverThreadState.clientsMutex = CreateMutex(NULL, FALSE, NULL);
	serverThreadState.serverSocket = serverSocket;
	serverThreadState.terminate = 0;

	HANDLE clientThread = CreateThread(NULL, 0, &serverLoop, &serverThreadState, 0, NULL);

	signal(SIGINT, ctrlCServerHandler);

	timer_t nextTimestampUpdateAt = 0.0f;
	timer_t nextTimestampBroadcastAt = 0.0f;
	while (!terminateServer) {
		const timer_t now = getHighPrecisionTime();

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
			broadcast(&serverThreadState, (const char*)&packet, packet.size, 0);

			nextTimestampBroadcastAt = now + randf(TIMESTAMP_BROADCAST_LOWER, TIMESTAMP_BROADCAST_UPPER);
		}
		// lese von Portaudio
		// baue Paket aus Zeit und Samples
		// sende an alle Clients
		// -> falls Client disconnected, hole Mutex und entferne aus Liste (das wird Scheiße, weil das ein Array ist... Schieberei?)

		printf("server time: %f\n", getTime(timerState));
	}

	serverThreadState.terminate = TRUE;
	WaitForSingleObject(clientThread, INFINITE);

	shutdown(serverSocket, SD_BOTH);
	// eigentlich: immer recv an alle Clients anfragen, bis alle mal 0 zurückgegeben haben, siehe http://msdn.microsoft.com/de-de/library/windows/desktop/ms740481(v=vs.85).aspx
	closesocket(serverSocket);

	return 0;
}
