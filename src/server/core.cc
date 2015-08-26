#include "connection.h"
#include "sim.h"

#include "../simlib/include/debug.h"
#include "../simlib/include/config_file.h"

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>

using std::string;

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

	ConfigFile config;
	config.addVar("address");
	config.addVar("workers");

	try {
		config.loadConfigFromFile("server.conf");
	} catch (const std::exception& e) {
		eprintf("Failed to load server.config: %s\n", e.what());
		return 5;
	}

	string address = config.getString("address");
	int workers = config.getInt("workers");

	if (workers < 1) {
		eprintf("sim.config: Number of workers cannot be lower than 1\n");
		return 6;
	}

	sockaddr_in name;
	name.sin_family = AF_INET;
	// Extract port from address
	unsigned port = 80; // server port
	size_t colon_pos = address.find(':');
	// Colon found
	if (colon_pos < address.size()) {
		if (strtou(address, &port, colon_pos + 1) <= 0) {
			eprintf("sim.config: wrong port number\n");
			return 7;
		}
		address[colon_pos] = '\0'; // need to extract IPv4 address
	} else
		address += '\0'; // need to extract IPv4 address
	// Set server port
	name.sin_port = htons(port);

	// Extract IPv4 address
	if (0 == strcmp(address.data(), "*")) // strcmp because of '\0' in address
		name.sin_addr.s_addr = htonl(INADDR_ANY); // server address
	else if (address.empty() ||
			inet_aton(address.data(), &name.sin_addr) == 0) {
		eprintf("sim.config: wrong IPv4 address\n");
		return 8;
	}

	E("address: %s:%u\n", address.data(), port);
	E("workers: %i\n", workers);

	if ((socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		eprintf("Failed to create socket - %s\n", strerror(errno));
		return 1;
	}

	bool true_ = 1;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &true_, sizeof(int))) {
		eprintf("Failed to setopt - %s\n", strerror(errno));
		return 2;
	}

	if (bind(socket_fd, (sockaddr*)&name, sizeof(name))) {
		eprintf("Failed to bind - %s\n", strerror(errno));
		return 3;
	}

	if (listen(socket_fd, 10)) {
		eprintf("Failed to listen - %s\n", strerror(errno));
		return 4;
	}

	pthread_t threads[workers];
	for (int i = 1; i < workers; ++i)
		pthread_create(threads + i, NULL, server::worker, NULL);
	threads[0] = pthread_self();
	server::worker(NULL);

	close(socket_fd);
	return 0;
}
