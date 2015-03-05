#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "../include/debug.h"
#include "../include/string.h"
#include "connection.h"
#include "sim.h"

using std::cerr;

#define WORKERS 1
static int socket_fd;

namespace server {
static void* worker(void*) {
	sockaddr_in name;
	Connection conn(-1);
	SIM sim_worker;
	for (;;) {
		socklen_t client_name_len = sizeof(name);
		// accept the connection
		int client_socket_fd = accept(socket_fd, (sockaddr*)&name, &client_name_len);

		char ip[INET_ADDRSTRLEN];

		inet_ntop(AF_INET, &name.sin_addr, ip, INET_ADDRSTRLEN); // extract ip

		eprintf("\nConnection accepted: %lu form %s\n", pthread_self(), ip);

		conn.assign(client_socket_fd);

		HttpRequest req = conn.getRequest();
		if (conn.state() == Connection::OK)
			conn.sendResponse(sim_worker.handle(ip, req));

		printf("Closing...");
		fflush(stdout);
		close(client_socket_fd);
		printf(" done.\n");
	}
	return NULL;
}
} // namespace server

int main() {
	srand(time(NULL));
	// Signal control
	struct sigaction sa;
	memset (&sa, 0, sizeof(sa));
	sa.sa_handler = &exit;

	sigaction(SIGINT, &sa, NULL);

	sockaddr_in name;

	name.sin_addr.s_addr = inet_addr("127.7.7.7"); //htonl(INADDR_ANY); // server address
	name.sin_port = htons(8080); // server port
	name.sin_family = AF_INET;
	if ((socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		eprintf("Failed to create socket\n");
		return 1;
	}
	bool true_ = 1;
	if (setsockopt(socket_fd,SOL_SOCKET,SO_REUSEADDR,&true_,sizeof(int))) {
		eprintf("Failed to setopt\n");
		return 4;
	}
	if (bind(socket_fd, (sockaddr*)&name, sizeof(name))) {
		eprintf("Failed to bind\n");
		return 2;
	}
	if (listen(socket_fd, 10)) {
		eprintf("Failed to listen\n");
		return 3;
	}
	pthread_t threads[WORKERS];
	for (int i = 1; i < WORKERS; ++i)
		pthread_create(threads + i, NULL, server::worker, NULL);
	threads[0] = pthread_self();
	server::worker(NULL);
	close(socket_fd);
	return 0;
}
