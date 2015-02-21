// #include <fcntl.h>
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

using std::cerr;

#define WORKERS 3
static int socket_fd;

namespace server {
static void* worker(void*) {
	sockaddr_in name;
	Connection conn(-1);
	for (;;) {
		socklen_t client_name_len = sizeof(name);
		// accept the connection
		int client_socket_fd = accept(socket_fd, (sockaddr*)&name, &client_name_len);

		eprintf("\nConnection accepted: %lu\n", pthread_self());

		conn.assign(client_socket_fd);
		while (conn.state() == Connection::OK) {
			conn.clear();
			HttpRequest req = conn.getRequest();
			eprintf("\nContent: '%s'\n", req.content.c_str());
			if (req.method == HttpRequest::POST) {
				// cerr << "form_data: " << req.form_data.other << endl;
				// cerr << "form_data -> files: " << req.form_data.files << endl;
			}
			if (conn.state() == Connection::OK) {
				HttpResponse resp;
				resp.headers["Content-Type"] = "text/html; charset=utf-8";
				resp.content = "<!DOCTYPE html>\n"
					"<html>\n"
					"<head>\n"
						"<title>Form</title>\n"
					"</head>\n"
					"<body>\n"
						// "<form method=\"POST\" enctype=\"text/plain\" action=\"http://127.7.7.7:8080\">\n"
						"<form method=\"POST\" enctype=\"multipart/form-data\" action=\"http://127.7.7.7:8080\">\n"
							"<input type=\"file\" name=\"file\">\n"
							"<input type=\"file\" name=\"file11\">\n"
							"<input type=\"text\" name=\"text\" value=\"golka = 1 12 = '\">\n"
							"<input type=\"submit\" value=\"Submit\">\n"
						"</form>\n"
					"</body>\n"
					"</html>";
				conn.sendResponse(resp);
			}
			break;
		}

		printf("Closing...");
		fflush(stdout);
		close(client_socket_fd);
		printf(" done.\n");
	}
	return NULL;
}
} // namespace server

int main() {
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
