#include "server.h" 

#include <stdlib.h>
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

/*
#define free(x) {printf("Freeing object %s at %p in file %s line %d\n", #x, x, __FILE__, __LINE__); fflush(stdout); free(x);}

void* jj_malloc(size_t size, char* objectname, char* file, int line) {
	void* p = malloc(size);
	printf("Mallocing object %s at %p in file %s line %d\n", objectname, p, file, line);
	fflush(stdout);
	return p;
}
#define malloc(x) jj_malloc(x, #x, __FILE__, __LINE__)
*/

volatile BOOL terminateServer;

typedef struct QueueElement {
	void* payload;
	int length;
	struct QueueElement* next;
} QueueElement;


typedef struct {
	QueueElement* head;
	HANDLE mutex;
	SOCKET socket;
	BOOL terminate;
	BOOL join;
} QueueWorkerState;

typedef struct {
	QueueWorkerState* workerState;
	HANDLE threadHandle;
} ClientData;

void broadcast(ClientData* clientData, char* data, const int length) {
	for (unsigned int clientIndex = 0; clientIndex < MAX_CLIENTS; clientIndex++) {
		QueueWorkerState* state = clientData[clientIndex].workerState;
		if (state) {
			QueueElement* queueElement = malloc(sizeof(QueueElement));
			if (!queueElement) {
				printf("could not allocate memory for queue element.\n");
				continue;
			}
			queueElement->payload = malloc(length);
			if (queueElement->payload) {
				memcpy(queueElement->payload, data, length);
				queueElement->length = length;
				queueElement->next = NULL;
				DWORD waitResult = WaitForSingleObject(state->mutex, INFINITE);
				if (waitResult == WAIT_OBJECT_0) {
					QueueElement* current = state->head;
					if (state->head) {
						while (current->next) {
							current = current->next;
						}
						current->next = queueElement;
					}
					else {
						state->head = queueElement;
					}
					ReleaseMutex(state->mutex);
				}
				else {
					printf("acquiring mutex failed with error: %u\n", waitResult);
				}
			}
			else {
				puts("cannot allocate memory for payload copy.");
				free(queueElement);
			}
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
		QueueElement* queueElement = state->head;
		if (queueElement == 0) {
			SwitchToThread();
		}
		else {
			const int sendResult = send(socket, queueElement->payload, queueElement->length, 0);
			if (sendResult == queueElement->length) {
				DWORD waitResult = WaitForSingleObject(state->mutex, INFINITE);
				if (waitResult == WAIT_OBJECT_0) {
					state->head = queueElement->next;
					free(queueElement->payload);
					free(queueElement);
				}
				else {
					printf("acquiring client worker queue mutex failed with error: %u\n", waitResult);
				}
				ReleaseMutex(state->mutex);
			}
			else if (sendResult == SOCKET_ERROR) {
				const int error = WSAGetLastError();
				if (error == WSAECONNRESET || error == WSAECONNABORTED) {
					printf("client disconnected\n");
					DWORD waitResult = WaitForSingleObject(state->mutex, INFINITE);
					state->join = TRUE;
					state->terminate = TRUE;
					ReleaseMutex(state->mutex);
				}
				else {
					printf("send failed with error: %d (tried to send %d bytes)\n", WSAGetLastError(), queueElement->length);
				}
			}
		}
	}
	return 0;
}

void deleteClientData(ClientData* cData) {
	WaitForSingleObject(cData->threadHandle, INFINITE);
	CloseHandle(cData->workerState->mutex);
	closeSocket(cData->workerState->socket);
	CloseHandle(cData->threadHandle);
	cData->threadHandle = NULL;
	QueueElement* current = cData->workerState->head;
	while (current) {
		QueueElement* next = current->next;
		free(current->payload);
		free(current);
		current = next;
	}
	free(cData->workerState);
	cData->workerState = NULL;
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

	SoundPacket* soundPacket = malloc(sizeof(SoundPacket));
	if (!soundPacket) {
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
				clientData[nextClientIndex].workerState = malloc(sizeof(QueueWorkerState));
				if (clientData[nextClientIndex].workerState) {

					clientData[nextClientIndex].workerState->mutex = CreateMutex(NULL, 0, NULL);

					if (clientData[nextClientIndex].workerState->mutex) {
						FD_SET(clientSocket, &clientSockets);
						printf("client accepted, now %u clients\n", clientSockets.fd_count);
						if (clientSockets.fd_count > FD_SETSIZE) {
							puts("maximum client size reached");
						}

						clientData[nextClientIndex].workerState->socket = clientSocket;
						clientData[nextClientIndex].workerState->terminate = FALSE;
						clientData[nextClientIndex].workerState->join = FALSE;
						clientData[nextClientIndex].workerState->head = 0;

						clientData[nextClientIndex].threadHandle = CreateThread(NULL, 0, workerThread, clientData[nextClientIndex].workerState, 0, NULL);
						nextClientIndex = (nextClientIndex + 1) % MAX_CLIENTS;
					}
					else {
						puts("could not create mutex for queue worker state.");
						free(clientData[nextClientIndex].workerState);
						clientData[nextClientIndex].workerState = NULL;
					}
				}
				else{
					puts("could not allocate memory queue worker state.");
				}
			}
		}


		if (now > nextTimestampUpdateAt) {
			const timer_t lowPrecisionTime = (timer_t)clock() / CLOCKS_PER_SEC;
			updateTimer(timerState, lowPrecisionTime);
			nextTimestampUpdateAt = now + randf(TIMESTAMP_UPDATE_LOWER, TIMESTAMP_UPDATE_UPPER);
		}

		if (now > nextTimestampBroadcastAt) {
			TimestampPacket timestampPacket;
			timestampPacket.type = PACKETTYPE_TIMESTAMP;
			timestampPacket.size = sizeof(TimestampPacket);
			timestampPacket.time = getTime(timerState);
			broadcast(clientData, (char*)&timestampPacket, timestampPacket.size);

			nextTimestampBroadcastAt = now + randf(TIMESTAMP_BROADCAST_LOWER, TIMESTAMP_BROADCAST_UPPER);
		}

		soundPacket->type = PACKETTYPE_SOUND;
		const PaError error = Pa_ReadStream(paStream, soundPacket->samples, FRAMES_PER_PACKET);
		if (error != paNoError) {
			printf("Pa_ReadStream failed with code: %d\n", error);
		}
		soundPacket->playTime = getTime(timerState) + PLAY_DELAY;
		soundPacket->size = sizeof(SoundPacket);
		broadcast(clientData, (char*)soundPacket, soundPacket->size);
		// -> falls Client disconnected, hole Mutex und entferne aus Liste (das wird Scheiße, weil das ein Array ist... Schieberei?)

		for (int i = 0; i < MAX_CLIENTS; ++i) {
			if (clientData[i].workerState) {
				WaitForSingleObject(clientData[i].workerState->mutex, INFINITE);
				if (clientData[i].workerState->join) {
					FD_CLR(clientData[i].workerState->socket, &clientSockets);
					deleteClientData(clientData + i);
				}
				else {
					ReleaseMutex(clientData[i].workerState->mutex);
				}
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
	free(soundPacket);

	return 0;
}
