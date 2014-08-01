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

#define MAX_CLIENTS 16

volatile BOOL terminateServer;

typedef struct QueueElement {
	void* payload;
	size_t length;
	struct QueueElement* next;
} QueueElement;


typedef struct {
	QueueElement** head;
	HANDLE mutex;
	SOCKET socket;
	BOOL terminate;
	BOOL join;
} QueueWorkerState;

typedef struct {
	QueueWorkerState* workerState;
	HANDLE threadHandle;
} ClientData;

void broadcast(fd_set* clients, const char* const data, const int length, const int flags) {
	for (unsigned int clientIndex = 0; clientIndex < clients->fd_count; clientIndex++) {
		SOCKET clientSocket = clients->fd_array[clientIndex];
		const int sendResult = send(clientSocket, data, length, flags);
		if (sendResult == SOCKET_ERROR) {
			const int error = WSAGetLastError();
			if (error == WSAECONNRESET || error == WSAECONNABORTED) {
				FD_CLR(clientSocket, clients);
				printf("client disconnected, now %d clients\n", clients->fd_count);
			}
			else {
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


DWORD WINAPI workerThread(void* param) {
	QueueWorkerState* state = (QueueWorkerState*)param;
	SOCKET socket = state->socket;
	while (!state->terminate) {
		QueueElement* queueElement = *state->head;
		
		void* packet = queueElement->payload;
		const int sendResult = send(socket, packet, queueElement->length, 0);
		if (sendResult == queueElement->length) {
			DWORD waitResult = WaitForSingleObject(state->mutex, INFINITE);
			if (waitResult == WAIT_OBJECT_0) {
				(*state->head) = queueElement->next;
				free(queueElement->payload);
				free(queueElement);
			}
			else {
				printf("acquiring client worker queue mutex failed with error: %d\n", waitResult);
				return 1;
			}
			ReleaseMutex(state->mutex);
		}
		else if (sendResult == SOCKET_ERROR) {
			const int error = WSAGetLastError();
			if (error == WSAECONNRESET || error == WSAECONNABORTED) {
				printf("client disconnected, now %d clients\n", clients->fd_count);
			}
			else {
				printf("broadcast send failed with error: %d (tried to send %d bytes)\n", WSAGetLastError(), length);
			}
		}
	}
	return 0;
}

void deleteClientData(ClientData* cData) {
	WaitForSingleObject(cData->threadHandle);
	CloseHandle(cData->workerState->mutex);
	closeSocket(cData->workerState->socket);
	CloseHandle(cData->threadHandle);
	QueueElement* current = *(cData->workerState->head);
	while (current) {
		QueueElement* next = current->next;
		free(current->payload);
		free(current);
		current = next;
	}
	free(cData->workerState->head);
	free(cData->workerState);
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

	fd_set clientSockets;
	FD_ZERO(&clientSockets);
	ClientData clientData[MAX_CLIENTS];
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		clientData[i].workerState = 0;
		clientData[i].threadHandle = 0;
	}
	int nextClientIndex = 0;
	
	timer_t nextTimestampUpdateAt = 0.0f;
	timer_t nextTimestampBroadcastAt = 0.0f;

	Pa_Initialize();
	const PaDeviceIndex defaultInputDevice = Pa_GetDefaultInputDevice();
	PaStream* paStream = setupStream(defaultInputDevice, -1);

	
	signal(SIGINT, ctrlCServerHandler);
	while (!terminateServer) {
		const timer_t now = getHighPrecisionTime();

		printf("now: %f\n", getTime(timerState));

		fd_set readFs;
		FD_ZERO(&readFs);
		FD_SET(serverSocket, &readFs);

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
				FD_SET(clientSocket, &clientSockets);
				printf("client accepted, now %d clients\n", clientSockets.fd_count);
				if (clientSockets.fd_count > FD_SETSIZE) {
					puts("maximum client size reached");
				}

				clientData[nextClientIndex].workerState = malloc(sizeof(QueueWorkerState));
				clientData[nextClientIndex].workerState->mutex = CreateMutex(NULL, 0, NULL);
				clientData[nextClientIndex].workerState->socket = clientSocket;
				clientData[nextClientIndex].workerState->terminate = FALSE;
				clientData[nextClientIndex].workerState->join = FALSE;
				clientData[nextClientIndex].workerState->head = malloc(sizeof(QueueElement*));
				*(clientData[nextClientIndex].workerState->head) = 0;

				clientData[nextClientIndex].threadHandle = CreateThread(NULL, 0, workerThread, clientData[nextClientIndex].workerState, 0, NULL);
				nextClientIndex = (nextClientIndex + 1) % MAX_CLIENTS;
				// TODO: Deleting clients and joining threads upon execution
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
			broadcast(&clientSockets, (const char*)&packet, packet.size, 0);

			nextTimestampBroadcastAt = now + randf(TIMESTAMP_BROADCAST_LOWER, TIMESTAMP_BROADCAST_UPPER);
		}

		packet->type = PACKETTYPE_SOUND;
		const PaError error = Pa_ReadStream(paStream, packet->samples, FRAMES_PER_PACKET);
		if (error != paNoError) {
			printf("Pa_ReadStream failed with code: %d\n", error);
		}
		packet->playTime = getTime(timerState) + PLAY_DELAY;
		packet->size = sizeof(SoundPacket);
		broadcast(&clientSockets, (const char*)packet, packet->size, 0);
		// -> falls Client disconnected, hole Mutex und entferne aus Liste (das wird Scheiße, weil das ein Array ist... Schieberei?)

		for (int i = 0; i < MAX_CLIENTS; ++i) {
			if (clientData[i].workerState) {
				WaitForSingleObject(clientData[i].workerState->mutex);
				if (clientData[i].workerState->join)
					deleteClientData(clientData + i);
				ReleaseMutex(clientData[i].workerState->mutex);
			}
		}
	}
		
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		if (clientData[i].workerState)
			clientData[i].workerState->terminate = TRUE;
		deleteClientData(clientData + i);
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
