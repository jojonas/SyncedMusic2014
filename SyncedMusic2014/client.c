#include "client.h"

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>

#include "network.h"
#include "server.h"

#define NETWORK_BUFFER_SIZE 1024*1024

volatile BOOL terminateClient;

void ctrlCClientHandler(int signal) {
	if (signal == SIGINT) {
		terminateClient = TRUE;
	}
}

typedef struct AudioQueue {
	timer_t playAt;
	unsigned char samples[AUDIO_BUFFER_SIZE];
	struct AudioQueue* next;
} AudioQueue;

void setConsoleColor(WORD attribute) {
	static HANDLE console = 0;
	static WORD savedAttributes;

	if (console == 0) {
		console = GetStdHandle(STD_OUTPUT_HANDLE);
		CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
		GetConsoleScreenBufferInfo(console, &consoleInfo);
		savedAttributes = consoleInfo.wAttributes;
	}
	if (attribute) {
		SetConsoleTextAttribute(console, attribute);
	}
	else {
		SetConsoleTextAttribute(console, savedAttributes);
	}
}

int clientMain(int argc, char** argv)
{
	assert(AUDIO_BUFFER_SIZE <= NETWORK_BUFFER_SIZE);

	PaStream* paStream = NULL;
	TimerState* timerState = NULL;
	SOCKET clientSocket = INVALID_SOCKET;

	const char* serverAddress = argv[1];

	terminateClient = FALSE;
	signal(SIGINT, ctrlCClientHandler);

	puts("info:");
	printAudioDeviceList();

	puts("Initiated client mode.");

	char* buffer = malloc(NETWORK_BUFFER_SIZE*sizeof(char));
	if (!buffer) {
		printf("could not allocate %d bytes for network buffer\n", NETWORK_BUFFER_SIZE);
		goto shutdown;
	}

	while (!terminateClient && clientSocket == INVALID_SOCKET) {
		printf("connecting to %s...\n", serverAddress);
		clientSocket = setupConnection(serverAddress, SERVER_PORT);
	}

	unsigned int dataEndIndex = 0;

	timerState = createTimer();
	if (timerState == NULL) {
		printf("could not create timer\n");
		goto shutdown;
	}

	timer_t individualDelay = 0;
	if (argc > 2) {
		individualDelay = atof(argv[2]);
	}

	Pa_Initialize();
	PaDeviceIndex defaultOutputDevice = Pa_GetDefaultOutputDevice();
	if (argc > 3) {
		defaultOutputDevice = atoi(argv[3]);
	}
	paStream = setupStream(-1, defaultOutputDevice);

	AudioQueue* head = 0;
	int queueLength = 0;

	clock_t lastReceive = clock();

	while (!terminateClient) {
		fd_set readFs;
		FD_ZERO(&readFs);
		FD_SET(clientSocket, &readFs);

		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;

		const int readySocketCount = select(0, &readFs, NULL, NULL, &timeout);
		if (readySocketCount >= 1) {
			const int bytesReceived = recv(clientSocket, buffer + dataEndIndex, NETWORK_BUFFER_SIZE - dataEndIndex, 0);

			if (bytesReceived == SOCKET_ERROR) {
				const int error = WSAGetLastError();
				printf("recv failed with error: %d\n", error);
				if (error == WSAECONNRESET || error == WSAECONNABORTED) {
					puts("connection reset by peer, reconnecting...");

					clientSocket = INVALID_SOCKET;
					while (!terminateClient && clientSocket == INVALID_SOCKET) {
						printf("reconnecting to %s...\n", serverAddress);
						clientSocket = setupConnection(serverAddress, SERVER_PORT);
						timerState->full = 0;
						timerState->nextPoint = 0;
						timerState->dirty = 1;
					}
					continue;
				}
				else {
					continue;
				}
			}
			else {
				lastReceive = clock();
				dataEndIndex += bytesReceived;
			}
		}
		else if (readySocketCount == SOCKET_ERROR) {
			const int error = WSAGetLastError();
			printf("select failed with error: %d\n", error);
		}
		else {
			if (clock() > lastReceive + CLIENT_TIMEOUT*CLOCKS_PER_SEC) {
				puts("client timeout");
				clientSocket = INVALID_SOCKET;
				while (!terminateClient && clientSocket == INVALID_SOCKET) {
					printf("reconnecting to %s...\n", serverAddress);
					clientSocket = setupConnection(serverAddress, SERVER_PORT);
					timerState->full = 0;
					timerState->nextPoint = 0;
					timerState->dirty = 1;
				}
				dataEndIndex = 0;
				lastReceive = clock();

				continue;
			}
		}

		unsigned int size;
		while (dataEndIndex > 0 && (size = *(unsigned int*)buffer) <= dataEndIndex) {
			//printf("client packet size: %d\n", size);
			// paket ganz!
			const PacketType type = *(PacketType*)(buffer + sizeof(unsigned int));
			switch (type) {
			case PACKETTYPE_TIMESTAMP: 
				{
					TimestampPacket* packet = (TimestampPacket*)buffer;
					setConsoleColor(FOREGROUND_RED);
					updateTimer(timerState, packet->time);
					setConsoleColor(0);
				}
				break;
			case PACKETTYPE_SOUND:
				{
					SoundPacket* packet = (SoundPacket*)buffer;
					AudioQueue* queueElement = malloc(sizeof(AudioQueue));
					queueElement->playAt = packet->playTime + individualDelay;
					queueElement->next = 0;
					memcpy(queueElement->samples, packet->samples, AUDIO_BUFFER_SIZE);
					
					if (head) { // TODO: maybe optimize? introduce "tail"-variable?
						AudioQueue* current = head;
						while (current->next) current = current->next;
						current->next = queueElement;
					}
					else {
						head = queueElement;
					}

					queueLength++;
				}
				break;
			default:
				printf("Invalid packet type: %d\n", type);
				break;
			}

			memmove(buffer, buffer + size, dataEndIndex - size);
			dataEndIndex -= size;
		}

		timer_t waitUntilPlay = head ? head->playAt - getTime(timerState) : 1.0;
		if (waitUntilPlay <= 0.0) { // packet in front of queue is (over)due
			int droppedFrames = (int)(-waitUntilPlay*SAMPLE_RATE);
			printf("wait: %f ms, dropped frames: %d, left: %d\n", waitUntilPlay*1000.0, droppedFrames, FRAMES_PER_PACKET - droppedFrames);
			unsigned char* samplesStart = (unsigned char*)head->samples + NUM_CHANNELS*BYTES_PER_SAMPLE*droppedFrames;
			if (FRAMES_PER_PACKET - droppedFrames > 0) {
				PaError error = Pa_WriteStream(paStream, samplesStart, FRAMES_PER_PACKET - droppedFrames); // doesn't block!
				if (error != paNoError) {
					printf("Pa_WriteStream failed with error: %d\n", error);
				}
			}

			AudioQueue* temp = head;
			head = head->next;
			free(temp);

			queueLength--;
			printf("queueLength: %d\n", queueLength);
			
			setConsoleColor(FOREGROUND_GREEN);
			printf("now: %f\n", getTime(timerState));
			setConsoleColor(0);
		}
	}



shutdown:
	Pa_AbortStream(paStream);
	Pa_CloseStream(paStream);
	Pa_Terminate();

	
	shutdown(clientSocket, SD_SEND);
	if (buffer) {
		int bytesReceived;
		do {
			bytesReceived = recv(clientSocket, buffer, NETWORK_BUFFER_SIZE, 0);
			if (bytesReceived == SOCKET_ERROR) {
				puts("graceful socket shutdown failed");
				break;
			}
		} while (bytesReceived != 0);
	}
	closesocket(clientSocket);

	free(timerState);
	free(buffer);

	return 0;
}
