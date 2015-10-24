#include "connection.h"
#include "sim.h"

#include "../simlib/include/config_file.h"
#include "../simlib/include/filesystem.h"
#include "../simlib/include/logger.h"
#include "../simlib/include/process.h"

#include <arpa/inet.h>
#include <csignal>

using std::string;

static int socket_fd;

namespace server {

static void* worker(void*) {
	try {
		sockaddr_in name;
		socklen_t client_name_len = sizeof(name);
		char ip[INET_ADDRSTRLEN];
		Connection conn(-1);
		Sim sim_worker;

		for (;;) {
			// accept the connection
			int client_socket_fd = accept(socket_fd, (sockaddr*)&name,
				&client_name_len);
			Closer closer(client_socket_fd);
			if (client_socket_fd == -1)
				continue;

			// extract IP
			inet_ntop(AF_INET, &name.sin_addr, ip, INET_ADDRSTRLEN);
			stdlog("Connection accepted: ", toString(pthread_self()), " form ",
				ip);

			conn.assign(client_socket_fd);
			HttpRequest req = conn.getRequest();

			if (conn.state() == Connection::OK)
				conn.sendResponse(sim_worker.handle(ip, req));

			stdlog("Closing...");
			closer.close();
			stdlog("Closed");
		}

	} catch (const std::exception& e) {
		error_log("Caught exception: ", __FILE__, ':', toString(__LINE__),
			" - ", e.what());

	} catch (...) {
		error_log("Caught exception: ", __FILE__, ':', toString(__LINE__));
	}

	return nullptr;
}

} // namespace server

int main() {
	// Init server
	srand(time(nullptr));
	// Change directory to process executable directory
	string cwd;
	try {
		cwd = chdirToExecDir();
	} catch (const std::exception& e) {
		error_log("Failed to change working directory: ", e.what());
	}

	// Loggers
	// stdlog like everything writes to stderr
	if (freopen("server.log", "a", stderr) == NULL)
		error_log("Failed to open 'server.log'", error(errno));

	try {
		error_log.open("server_error.log");
	} catch (const std::exception& e) {
		error_log("Failed to open 'server_error.log': ", e.what());
	}

	// Signal control
	struct sigaction sa;
	memset (&sa, 0, sizeof(sa));
	sa.sa_handler = &exit;

	sigaction(SIGINT, &sa, nullptr);
	sigaction(SIGTERM, &sa, nullptr);
	sigaction(SIGQUIT, &sa, nullptr);

	ConfigFile config;
	try {
		config.addVar("address");
		config.addVar("workers");

		config.loadConfigFromFile("server.conf");
	} catch (const std::exception& e) {
		error_log("Failed to load server.config: ", e.what());
		return 5;
	}

	string address = config.getString("address");
	int workers = config.getInt("workers");

	if (workers < 1) {
		error_log("sim.config: Number of workers cannot be lower than 1");
		return 6;
	}

	sockaddr_in name;
	name.sin_family = AF_INET;
	memset(name.sin_zero, 0, sizeof(name.sin_zero));

	// Extract port from address
	unsigned port = 80; // server port
	size_t colon_pos = address.find(':');
	// Colon found
	if (colon_pos < address.size()) {
		if (strtou(address, &port, colon_pos + 1) <= 0) {
			error_log("sim.config: incorrect port number");
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
		error_log("sim.config: incorrect IPv4 address");
		return 8;
	}

	stdlog("Server launch:\n"
		"PID: ", toString(getpid()), "\n"
		"workers: ", toString(workers), "\n"
		"address: ", address.data(), ':', toString(port));

	if ((socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		error_log("Failed to create socket", error(errno));
		return 1;
	}

	bool true_ = true;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &true_, sizeof(int))) {
		error_log("Failed to setopt", error(errno));
		return 2;
	}

	if (bind(socket_fd, (sockaddr*)&name, sizeof(name))) {
		error_log("Failed to bind", error(errno));
		return 3;
	}

	if (listen(socket_fd, 10)) {
		error_log("Failed to listen", error(errno));
		return 4;
	}

	pthread_t threads[workers];
	for (int i = 1; i < workers; ++i)
		pthread_create(threads + i, nullptr, server::worker, nullptr);
	threads[0] = pthread_self();
	server::worker(nullptr);

	sclose(socket_fd);
	return 0;
}
