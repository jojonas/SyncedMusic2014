#include "server.h"
#include "client.h"

int main(int argc, char** argv) {
	if (argc <= 1) {
		serverMain(argc, argv);
	}
	else {
		clientMain(argc, argv);
	}
	return 0;
}

