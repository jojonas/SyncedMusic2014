#include "network.h"

#include <stdio.h>


SOCKET setupListeningSocket(const unsigned short port)
{
	int iResult;
	WSADATA wsaData;
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		WSACleanup();
		return INVALID_SOCKET;
	}

	struct addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	char buffer[16];
	_itoa_s(port, buffer, 16, 10);

	struct addrinfo* result;
	iResult = getaddrinfo(NULL, buffer, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return INVALID_SOCKET;
	}

	SOCKET serverSocket = INVALID_SOCKET;
	serverSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (serverSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return INVALID_SOCKET;
	}

	u_long iMode = 0; // blocking: 0, nonblocking: 1
	iResult = ioctlsocket(serverSocket, FIONBIO, &iMode);
	if (iResult != NO_ERROR) {
		printf("ioctlsocket failed with error: %d\n", iResult);
		freeaddrinfo(result);
		closesocket(serverSocket);
		WSACleanup();
		return INVALID_SOCKET;
	}

	iResult = bind(serverSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(serverSocket);
		WSACleanup();
		return INVALID_SOCKET;
	}

	freeaddrinfo(result);

	iResult = listen(serverSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(serverSocket);
		WSACleanup();
		return INVALID_SOCKET;
	}

	return serverSocket;
}

SOCKET setupConnection(const char* host, const int port)
{
	int iResult;
	WSADATA wsaData;
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		WSACleanup();
		return INVALID_SOCKET;
	}

	struct addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	char buffer[16];
	_itoa_s(port, buffer, 16, 10);

	struct addrinfo* result;
	iResult = getaddrinfo(host, buffer, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed: %d\n", iResult);
		WSACleanup();
		return INVALID_SOCKET;
	}

	SOCKET clientSocket = INVALID_SOCKET;
	clientSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (clientSocket == INVALID_SOCKET) {
		printf("Error at socket(): %d\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return INVALID_SOCKET;
	}

	u_long iMode = 0; // blocking: 0, nonblocking: 1
	iResult = ioctlsocket(clientSocket, FIONBIO, &iMode);
	if (iResult != NO_ERROR) {
		printf("ioctlsocket failed with error: %d\n", iResult);
		freeaddrinfo(result);
		closesocket(clientSocket);
		WSACleanup();
		return INVALID_SOCKET;
	}

	struct timeval recvTimeout;
	recvTimeout.tv_sec = 2;
	recvTimeout.tv_usec = 0;

	iResult = setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&recvTimeout, sizeof(recvTimeout));
	if (iResult == SOCKET_ERROR) {
		printf("setsockopt for SO_RCVTIMEO failed with error: %u\n", WSAGetLastError());
		closesocket(clientSocket);
		freeaddrinfo(result);
		WSACleanup();
		return INVALID_SOCKET;
	}

	iResult = connect(clientSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("Error at connect: %d\n", WSAGetLastError());
		closesocket(clientSocket);
		freeaddrinfo(result);
		WSACleanup();
		return INVALID_SOCKET;
	}
	return clientSocket;
}

void closeSocket(SOCKET socket)
{
	char buffer[1024];
	if (socket != INVALID_SOCKET) {
		shutdown(socket, SD_SEND);
		int bytesReceived;
		do {
			bytesReceived = recv(socket, buffer, 1024, 0);
			if (bytesReceived == SOCKET_ERROR) {
				puts("graceful socket shutdown failed");
				break;
			}
		} while (bytesReceived != 0);
	}
	closesocket(socket);
}
