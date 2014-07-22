#include "server.h" 

#include <stdio.h>
#include <signal.h>

#include "network.h"
#include "audio.h"

#define SERVER_PORT 1337

volatile BOOL terminateMain;

typedef struct {
	fd_set* clients;
	HANDLE clientsMutex;
	SOCKET serverSocket;
	volatile BOOL terminate;
} ServerThreadState;

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
			}
			else {
				printf("cannot acquire mutex, error: %d\n", waitResult);
				return 1;
			}
		}
	}
	return 0;
}

void ctrlCHandler(int signal) {
	if (signal == SIGINT) {
		terminateMain = TRUE;
	}
}


int serverMain(int argc, char** argv)
{
	char* networkBuffer = malloc(4096 * sizeof(char));

	terminateMain = FALSE;

	SOCKET serverSocket = setupListeningSocket(SERVER_PORT);
	if (serverSocket == INVALID_SOCKET) {
		printf("could not set up listening socket\n");
		return 1;
	}
	fd_set clients;

	ServerThreadState serverThreadState;
	serverThreadState.clients = &clients;
	serverThreadState.clientsMutex = CreateMutex(NULL, FALSE, NULL);
	serverThreadState.serverSocket = serverSocket;
	serverThreadState.terminate = 0;

	HANDLE clientThread = CreateThread(NULL, 0, &serverLoop, &serverThreadState, 0, NULL);

	signal(SIGINT, ctrlCHandler);

	while (!terminateMain) {
		// lese von Portaudio
		// lese genaue Zeit
		// -> "ab und zu": kalibriere Zeit
		// baue Paket aus Zeit und Samples
		// sende an alle Clients
		// -> falls Client disconnected, hole Mutex und entferne aus Liste (das wird Scheiße, weil das ein Array ist... Schieberei?)
	}

	serverThreadState.terminate = TRUE;
	WaitForSingleObject(clientThread, INFINITE);

	shutdown(serverSocket, SD_BOTH);
	// eigentlich: immer recv an alle Clients anfragen, bis alle mal 0 zurückgegeben haben, siehe http://msdn.microsoft.com/de-de/library/windows/desktop/ms740481(v=vs.85).aspx
	closesocket(serverSocket);

	return 0;
}
