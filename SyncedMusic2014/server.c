#include "server.h" 

#include <stdio.h>
#include "network.h"
#include "audio.h"

#define SERVER_PORT 1337


typedef struct {
	fd_set* clients;
	SOCKET serverSocket;
	int terminate;
} ServerThreadState;

DWORD WINAPI serverLoop(void* parameter) {
	ServerThreadState* state = parameter;
	while (!state->terminate) {
		SOCKET clientSocket = accept(state->serverSocket, NULL, NULL);
		if (clientSocket == INVALID_SOCKET) {
			printf("accept failed: %d\n", WSAGetLastError());
		}
		else {
			puts("Client accepted");
			state->clients->fd_array[state->clients->fd_count] = clientSocket;
			state->clients->fd_count++;
		}
	}
}


int serverMain(int argc, char** argv)
{
	printDeviceInfo();
	/*SOCKET serverSocket = setupListeningSocket(SERVER_PORT);
	fd_set clients;

	ServerThreadState serverThreadState;
	serverThreadState.clients = &clients;
	serverThreadState.serverSocket = serverSocket;
	serverThreadState.terminate = 0;

	CreateThread(NULL, 0, &serverLoop, &serverThreadState, 0, NULL);

	while (1) {
		//selects durchführen auf alle 
	}*/
}
