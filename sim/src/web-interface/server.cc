#include "connection.h"
#include "sim.h"

#include <arpa/inet.h>
#include <chrono>
#include <csignal>
#include <pthread.h>
#include <simlib/config_file.h>
#include <simlib/debug.h>
#include <simlib/filesystem.h>
#include <simlib/process.h>

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
			stdlog("Connection accepted: ", pthread_self(), " form ", ip);

			conn.assign(client_socket_fd);
			HttpRequest req = conn.getRequest();

			if (conn.state() == Connection::OK) {
				using namespace std::chrono;
				auto beg = steady_clock::now();

				HttpResponse resp = sim_worker.handle(ip, std::move(req));

				auto microdur = duration_cast<microseconds>
					(steady_clock::now() - beg).count();
				stdlog("Response generated in ", toString(microdur / 1000.0, 3),
					" ms.");

				conn.sendResponse(std::move(resp));
			}

			stdlog("Closing...");
			closer.close();
			stdlog("Closed");
		}

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);

	} catch (...) {
		ERRLOG_CATCH();
	}

	return nullptr;
}

} // namespace server

int main() {
	// Init server
	// Change directory to process executable directory
	string cwd;
	try {
		cwd = chdirToExecDir();
	} catch (const std::exception& e) {
		errlog("Failed to change working directory: ", e.what());
	}

	// Loggers
	// stdlog like everything writes to stderr, so redirect stdout and stderr to
	// the log file
	if (freopen(SERVER_LOG, "a", stdout) == nullptr ||
		dup2(STDOUT_FILENO, STDERR_FILENO) == -1)
	{
		errlog("Failed to open `", SERVER_LOG, '`', error());
	}

	try {
		errlog.open(SERVER_ERROR_LOG);
	} catch (const std::exception& e) {
		errlog("Failed to open `", SERVER_ERROR_LOG, "`: ", e.what());
	}

	// Signal control
	struct sigaction sa;
	memset (&sa, 0, sizeof(sa));
	sa.sa_handler = &exit;

	(void)sigaction(SIGINT, &sa, nullptr);
	(void)sigaction(SIGTERM, &sa, nullptr);
	(void)sigaction(SIGQUIT, &sa, nullptr);

	ConfigFile config;
	try {
		config.addVars("address", "workers");

		config.loadConfigFromFile("server.conf");
	} catch (const std::exception& e) {
		errlog("Failed to load server.config: ", e.what());
		return 5;
	}

	string address = config["address"].asString();
	int workers = config["workers"].asInt();

	if (workers < 1) {
		errlog("sim.config: Number of workers cannot be lower than 1");
		return 6;
	}

	sockaddr_in name;
	name.sin_family = AF_INET;
	memset(name.sin_zero, 0, sizeof(name.sin_zero));

	// Extract port from address
	unsigned port = 80; // server port
	size_t colon_pos = address.find(':');
	// Colon has been found
	if (colon_pos < address.size()) {
		if (strtou(address, port, colon_pos + 1) !=
			static_cast<int>(address.size() - colon_pos - 1))
		{
			errlog("sim.config: incorrect port number");
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
	else if (address.empty() || inet_aton(address.data(), &name.sin_addr) == 0)
	{
		errlog("sim.config: incorrect IPv4 address");
		return 8;
	}

	stdlog("\n=================== Server launched ===================\n"
		"PID: ", getpid(), "\n"
		"workers: ", workers, "\n"
		"address: ", address.data(), ':', port);

	if ((socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		errlog("Failed to create socket", error());
		return 1;
	}

	int true_ = 1;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &true_, sizeof(int))) {
		errlog("Failed to setopt", error());
		return 2;
	}

	// Bind
	constexpr int TRIES = 8;
	for (int try_no = 1; bind(socket_fd, (sockaddr*)&name, sizeof(name)); ) {
		errlog("Failed to bind (try ", try_no, ')', error());
		if (++try_no > TRIES)
			return 3;

		usleep(800000);
	}

	if (listen(socket_fd, 10)) {
		errlog("Failed to listen", error());
		return 4;
	}

	// Alter default thread stack size
	pthread_attr_t attr;
	constexpr size_t THREAD_STACK_SIZE = 4 << 20; // 4 MB
	if (pthread_attr_init(&attr) ||
		pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE))
	{
		errlog("Failed to set new thread stack size");
		return 4;
	}

	pthread_t threads[workers];
	for (int i = 1; i < workers; ++i)
		pthread_create(threads + i, &attr, server::worker, nullptr); // TODO: errors...
	threads[0] = pthread_self();
	server::worker(nullptr);

	sclose(socket_fd);
	return 0;
}
