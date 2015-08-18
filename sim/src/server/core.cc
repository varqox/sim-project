#include "connection.h"
#include "sim.h"

#include "../simlib/include/debug.h"

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstring>

using std::cerr;

#define WORKERS 1
static int socket_fd;

namespace server {

static void* worker(void*) {
	try {
		sockaddr_in name;
		Connection conn(-1);
		Sim sim_worker;

		for (;;) {
			socklen_t client_name_len = sizeof(name);
			// accept the connection
			int client_socket_fd = accept(socket_fd, (sockaddr*)&name,
				&client_name_len);
			char ip[INET_ADDRSTRLEN];

			// extract IP
			inet_ntop(AF_INET, &name.sin_addr, ip, INET_ADDRSTRLEN);
			eprintf("\nConnection accepted: %lu form %s\n", pthread_self(), ip);

			conn.assign(client_socket_fd);
			HttpRequest req = conn.getRequest();

			if (conn.state() == Connection::OK)
				conn.sendResponse(sim_worker.handle(ip, req));

			eprintf("Closing...");
			fflush(stdout);
			close(client_socket_fd);
			eprintf(" done.\n");
		}

	} catch (const std::exception& e) {
		E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__, __LINE__,
			e.what());

	} catch (...) {
		E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
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
	name.sin_addr.s_addr = inet_addr("127.7.7.7"); // htonl(INADDR_ANY); // server address
	name.sin_port = htons(8080); // server port
	name.sin_family = AF_INET;

	if ((socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		eprintf("Failed to create socket - %s\n", strerror(errno));
		return 1;
	}

	bool true_ = 1;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &true_, sizeof(int))) {
		eprintf("Failed to setopt - %s\n", strerror(errno));
		return 4;
	}

	if (bind(socket_fd, (sockaddr*)&name, sizeof(name))) {
		eprintf("Failed to bind - %s\n", strerror(errno));
		return 2;
	}

	if (listen(socket_fd, 10)) {
		eprintf("Failed to listen - %s\n", strerror(errno));
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
