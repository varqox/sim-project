#include "connection.h"
#include "sim.h"

#include <arpa/inet.h>
#include <set>
#include <simlib/config_file.h>
#include <simlib/debug.h>
#include <simlib/process.h>
#include <sys/epoll.h>
#include <sys/resource.h>

using std::array;
using std::string;
using std::unique_ptr;
using std::vector;

static int socket_fd; // Major socket
static int workers; // Number of workers
static int epoll_fd; // epoll(7) instance
static unsigned connections; // number of connections

namespace {

struct Headers {
	size_t content_length = 0;
	FixedString connection = "keep-alive";
	FixedString host;
	FixedString user_agent;
	vector<std::pair<FixedString, FixedString>> other;

	Headers() = default;

	Headers(const Headers&) = delete; // TODO: default;
	Headers(Headers&&) = default;
	Headers& operator=(const Headers&) = delete; // TODO: default;
	Headers& operator=(Headers&&) = default;

	// Returns value of header with name @p name or empty string if such a
	// header does not exist
	StringView find(const StringView& name) {
		decltype(other.begin()) beg = other.begin(), end = other.end(), mid;
		while (beg < end) {
			mid = beg + ((end - beg) >> 1);
			if (special_less<int(int)>(mid->first, name, tolower))
				beg = ++mid;
			else
				end = mid;
		}
		return (mid == other.end() || lower_equal(mid->first, name)
			? "" : mid->second);
	}
};

struct Request {
	enum Method : uint8_t { GET, POST } method;
	FixedString target, http_version;
	Headers headers;
	FixedString content;
};

class Connection {
	static constexpr size_t MAX_HEADER_LENGTH = 8192;
	static constexpr uint8_t MAX_HEADERS_NO = 100; // be careful with type (see
	                                               // field: header_no)
	static constexpr size_t MAX_NON_FORM_CONTENT_LENGTH = 1 << 20; // 1 MiB

	int fd_;
	in_addr sin_addr_; // Client address
	unsigned short port_; // Client port

	Request req_; // Currently parsed request
	string buff_;
	uint8_t header_no = 0;
	uint8_t making_headers:1;

public:
	uint8_t idle:1;
	uint8_t reading:1;
	uint8_t writing:1;

	vector<Request> requests;

	Connection(int fde, const sockaddr_in& sock_addr, bool idl = true,
		bool rd = false, bool wr = false)
		: fd_(fde), sin_addr_(sock_addr.sin_addr),
			port_(ntohs(sock_addr.sin_port)), making_headers(true), idle(idl),
			reading(rd), writing(wr) {}

	Connection(const Connection&) = delete;

	Connection(Connection&&) = default; // TODO

	Connection& operator=(const Connection&) = delete;

	Connection& operator=(Connection&&) = default; // TODO

	~Connection() {};

	int fd() const noexcept { return fd_; }

	const char* ip(char* s) const noexcept {
		return inet_ntop(AF_INET, &sin_addr_, s, INET_ADDRSTRLEN);
	}

	FixedString ip() const {
		char s[INET_ADDRSTRLEN];
		(void)inet_ntop(AF_INET, &sin_addr_, s, INET_ADDRSTRLEN);
		return s;
	}

	unsigned short port() const noexcept { return port_; }

	void parse(const char* str, size_t len);

private:
	// Parses @p str header to construct @p name and value on it; returns false
	// on success, true on error
	static bool parseHeader(StringView str, StringView& name,
		StringView& value);

	// Constructs headers, returns false on success, true otherwise (and may add
	// proper response to TODO: where?)
	bool constructHeaders(StringView& data);
};

namespace {

struct ConnectionCompare {
	bool operator()(const Connection& a, const Connection& b) const {
		return a.fd() < b.fd();
	}
};

} // anonymous namespace

class ConnectionSet : private std::set<Connection, ConnectionCompare> {
	typedef std::set<Connection, ConnectionCompare> BaseType;

public:
	ConnectionSet() = default;
	~ConnectionSet() = default;

	ConnectionSet(const ConnectionSet&) = delete;
	ConnectionSet(ConnectionSet&&) = delete;
	ConnectionSet& operator=(const ConnectionSet&) = delete;
	ConnectionSet& operator=(ConnectionSet&&) = delete;

	using BaseType::begin;
	using BaseType::emplace;
	using BaseType::end;
	using BaseType::iterator;
	using BaseType::const_iterator;

	iterator find(int fd) {
		return BaseType::find(Connection(fd, {0, 0, {0}, {}}));
	}

	const_iterator find(int fd) const {
		return BaseType::find(Connection(fd, {0, 0, {0}, {}}));
	}

	void erase(int fd) { BaseType::erase(find(fd)); }

	void erase(iterator it) { BaseType::erase(it); }
};

} // anonymous namespace

static ConnectionSet connset; // set of connections

namespace {

class Job {
public:
	Job() = default;

	Job(const Job&) = delete;

	Job(Job&&) = delete;

	Job& operator=(const Job&) = delete;

	Job& operator=(Job&&) = delete;

	~Job() {};
};

} // anonymous namespace

#if 1
// # warning "Before committing disable this debug"
# define DEBUG_HEADERS(...) stdlog(__VA_ARGS__)
#else
# define DEBUG_HEADERS(...)
#endif

bool Connection::parseHeader(StringView str, StringView& name,
	StringView& value)
{
	name = str.substr(0, str.find(':'));
	if (name.find(' ') != StringView::npos || name.size() == 0
		|| name.size() == str.size())
	{
		return true; // Invalid header
	}

	str.removePrefix(name.size() + 1);
	str.removeLeading(isspace);
	str.removeTrailing(isspace);
	value = str;
	return false;
}

bool Connection::constructHeaders(StringView& data) {
	DEBUG_HEADERS("Connection ", toStr(fd_), " -> \033[1;33m"
		"Parsing:\033[m ", ConfigFile::escapeString(data, true));

	if (data.empty())
		return false;

	StringView header;
	const auto pick_next_header = [&]() -> bool {
		size_t x = data.find("\r\n");
		if (x == StringView::npos)
			return false; // Failure

		header = data.substr(0, x);
		data.removePrefix(x + 2);
		return true; // Success
	};

	// Sophisticated way to efficiently get first header
	if (buff_.size()) {
		if (buff_.back() == '\r' && data.front() == '\n') {
			header = StringView(buff_.data(), buff_.size() - 1);
			data.removePrefix(1);
		} else {
			size_t x = data.find("\r\n");
			if (x == StringView::npos) {
				buff_.append(data.data(), data.size());
				data.clear();
				return false;
			}

			buff_.append(data.data(), x);
			header = buff_;
			data.removePrefix(x + 2);
		}

	} else if (!pick_next_header()) { // No header is ready yet
		buff_.assign(data.data(), data.size());
		data.clear();
		return false;
	}

	do {
		DEBUG_HEADERS("Connection ", toStr(fd_),
			" -> Header: \033[36m", header, "\033[m");
		++header_no;

		/* Request line */
		if (header_no == 1) {
			if (header.empty()) { // Ignore empty requests
				header_no = 0;
				continue;
			}

			req_.headers = Headers();

			// Method
			size_t pos = 0;
			for (; pos < header.size() && !isspace(header[pos]); ++pos) {}

			if (header.substr(0, pos) == "GET")
				req_.method = Request::GET;
			else if (header.substr(0, pos) == "POST")
				req_.method = Request::POST;
			else {
				// TODO: error 501
				return true;
			}

			header.removePrefix(pos);

			// Target
			header.removeLeading(isspace);
			for (pos = 0; pos < header.size() && !isspace(header[pos]);
				++pos) {}
			req_.target = header.substr(0, pos);
			header.removePrefix(pos);

			// HTTP version
			header.removeLeading(isspace);
			if (header != "HTTP/1.0" && header != "HTTP/1.1") {
				// TODO: error 400
				return true;
			}
			req_.http_version = header;
			continue;
		}

		/* Headers limit */
		if (header_no > MAX_HEADERS_NO) {
			// TODO: error 413
			header_no = 0;
			return true;
		}

		/* Headers are complete */
		if (header.empty()) {
			DEBUG_HEADERS("Connection ", toStr(fd_),
				" -> \033[1;32mHeaders parsed.\033[m");

			std::sort(req_.headers.other.begin(), req_.headers.other.end());
			if (req_.headers.find("Expect") == "100-continue")
				{}// TODO: add response: 100

			if (req_.headers.content_length == 0)
				{}// TODO: add request to queue
			else
				making_headers = false;

			header_no = 0;
			buff_.clear();
			return false;
		}

		/* Normal header */

		if (header.size() > MAX_HEADER_LENGTH) {
			// TODO: error 431
			return true;
		}

		StringView name, value;
		if (parseHeader(header, name, value)) {
			// TODO: error 400
			return true;
		}

		// Put it into correct place
		if (lower_equal(name, "content-length")) {
			if (!isDigit(value)) {
				// TODO: error 400
				return true;
			}
			req_.headers.content_length = digitsToU<size_t>(value);
			if (req_.headers.content_length > MAX_NON_FORM_CONTENT_LENGTH) {
				// TODO: error 413
				return true;
			}

		} else if (lower_equal(name, "host"))
			req_.headers.host = value;
		else if (lower_equal(name, "connection"))
			req_.headers.connection = value;
		else if (lower_equal(name, "user-agent"))
			req_.headers.user_agent = value;
		else
			req_.headers.other.emplace_back(name, value);

	} while (pick_next_header());

	buff_.assign(data.data(), data.size());
	data.clear();
	return false;
}

void Connection::parse(const char *str, size_t len) {
	StringView data(str, len);
	while (data.size()) {
		if (making_headers) { // Headers
			constructHeaders(data);

		} else if (req_.method == Request::GET) { // Content (GET)
			size_t x = std::min(data.size(),
				req_.headers.content_length - buff_.size());
			buff_.append(data.data(), x);
			data.removePrefix(x);

			// Content is completely read
			if (req_.headers.content_length == buff_.size()) {
				// TODO: append request to queue
				making_headers = true;
				header_no = 0;
				buff_.clear();
			}

		} else if (req_.method == Request::POST) { // Content (POST)
			// TODO: implement
			data.clear();

		} else { // Bug
			errlog(__FILE__, ':', toStr(__LINE__), ": Nothing to parse - "
				"this is probably a bug");
			// TODO: connection state = CLOSE
			return;
		}
	}
}

#if 0
static void* worker(void*) {
	try {
		sockaddr_in name;
		socklen_t client_name_len = sizeof(name);
		char ip[INET_ADDRSTRLEN];
		server::Connection conn(-1);
		Sim sim_worker;

		for (;;) {
			// accept the connection
			int client_socket_fd = accept(socket_fd, (sockaddr*)&name,
				&client_name_len);
			Closer closer(client_socket_fd);
			if (client_socket_fd == -1)
				continue;

			// extract IP
			inet_ntop(AF_INET, &name, ip, INET_ADDRSTRLEN);
			stdlog("Connection accepted: ", toStr(pthread_self()), " form ",
				ip);

			conn.assign(client_socket_fd);
			server::HttpRequest req = conn.getRequest();

			if (conn.state() == server::Connection::OK)
				conn.sendResponse(sim_worker.handle(ip, req));

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
#endif

// Reads data from connections
static void* reader_thread(void*) {
	// Leave all signals to the master thread
	pthread_sigmask(SIG_SETMASK, &SignalBlocker::full_mask, nullptr);

	constexpr uint MAX_EVENTS = 64;
	constexpr uint MAX_ITERATIONS_PER_ONE = 32;

	array<epoll_event, MAX_EVENTS> events;
	array<char, 65536> buff;

	for (;;) {
		int n = epoll_wait(epoll_fd, events.data(), events.size(), -1);
		if (n < 0) {
			errlog(__FILE__, ':', toStr(__LINE__), ": epoll_wait()",
				error(errno));
			continue;
		}

		for (int i = 0; i < n; ++i) {
			D(stdlog("Event: ", toStr((int)events[i].data.fd), " -> ",
				(events[i].events & EPOLLIN ? "EPOLLIN | " : ""),
				(events[i].events & EPOLLOUT ? "EPOLLOUT | " : ""),
				(events[i].events & EPOLLRDHUP ? "EPOLLRDHUP | " : ""),
				(events[i].events & EPOLLPRI ? "EPOLLPRI | " : ""),
				(events[i].events & EPOLLERR ? "EPOLLERR | " : ""),
				(events[i].events & EPOLLHUP ? "EPOLLHUP | " : ""),
				(events[i].events & EPOLLET ? "EPOLLET | " : ""),
				(events[i].events & EPOLLONESHOT ? "EPOLLONESHOT | " : ""));)

			for (uint j = 0; j < MAX_ITERATIONS_PER_ONE; ++j) {
				ssize_t rc = read(events[i].data.fd, buff.data(), buff.size());
				D(stdlog("read: ", toStr(rc));)

				if (rc < 0)
					break;

				if (rc == 0) { // Connection was closed
					// TODO: create job to close connection
					(void)epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd,
						nullptr);
					break;
				}

				const_cast<Connection&>(*connset.find(events[i].data.fd))
					.parse(buff.data(), rc);
			}
		}
		// TODO: exceptions
	}
	return nullptr;
}

// Writes data to connections
static void* writer_thread(void*) {
	// Leave all signals to the master thread
	pthread_sigmask(SIG_SETMASK, &SignalBlocker::full_mask, nullptr);
	// TODO: exceptions

	return nullptr;
}
/*
// Manages workers
static void* workers_manager_thread(void*) {
	// Leave all signals to the master thread
	pthread_sigmask(SIG_SETMASK, &SignalBlocker::full_mask, nullptr);

	// TODO: exceptions
	return nullptr;
}*/

// Manages workers, accepts connections
static void master_process_cycle() {
	// epoll(7)
	epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	if (epoll_fd == -1) {
		errlog("Error: epoll_create1()", error(errno));
		exit(9);
	}

	// Run reader and writer threads
	pthread_t reader_pid, writer_pid;
	if (pthread_create(&reader_pid, nullptr, reader_thread, nullptr) == -1 ||
		pthread_create(&writer_pid, nullptr, writer_thread, nullptr) == -1)
	{
		errlog("Failed to spawn reader and writer threads", error(errno));
		exit(10);
	}

	// Adjust soft limit to hard limit of file descriptors
	struct rlimit limit;
	(void)getrlimit(RLIMIT_NOFILE, &limit);
	limit.rlim_cur = limit.rlim_max; // TODO: check out what happens when limit
	                                 // is reached
	(void)setrlimit(RLIMIT_NOFILE, &limit);

	stdlog("NOFILE limit: ", toStr(limit.rlim_max),
		"\n=================== Server launched ===================");
	stdlog.label(true);

	for (;;) {
		sockaddr_in name;
		socklen_t name_len = sizeof(name);
		// Accept connection
		int fd = accept4(socket_fd, (sockaddr*)&name, &name_len, SOCK_NONBLOCK |
			SOCK_CLOEXEC);
		if (fd == -1)
			continue;

		// Insert connection to connset
		auto x = connset.emplace(fd, name, false, true);
		throw_assert(x.second == true);
		const Connection &conn = *x.first;

		stdlog("Connection ", toStr(fd), " from ", conn.ip(), ':',
			toStr(conn.port()), " accepted");

		// Add connection epoll
		epoll_event event;
		memset(&event, 0, sizeof(event));
		event.events = EPOLLIN | EPOLLPRI;
		event.data.fd = fd;
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
			errlog("Error: epoll_ctl()", error(errno));
			// TODO: write error 500
			connset.erase(x.first);
			close(fd);
		}
	}
}

// Loads server configuration
static void loadServerConfig(const CStringView config_path,
	sockaddr_in& sock_name)
{
	ConfigFile config;
	try {
		config.addVars("address", "workers", "connections");

		config.loadConfigFromFile(config_path);
	} catch (const std::exception& e) {
		errlog("Failed to load ", config_path, ": ", e.what());
		exit(5);
	}

	const char* vars[] = {"address", "workers", "connections"};
	for (auto var : vars)
		if (!config[var].isSet()) {
			errlog(config_path, ": variable '", var, "' is not defined");
			exit(6);
		}

	string address = config["address"].asString();
	workers = config["workers"].asInt();
	if (workers < 1) {
		errlog(config_path, ": Number of workers cannot be lower than 1");
		exit(6);
	}

	connections = config["connections"].asInt();
	if (connections < 1) {
		errlog(config_path, ": Number of connections cannot be lower than 1");
		exit(6);
	}

	sock_name.sin_family = AF_INET;
	memset(sock_name.sin_zero, 0, sizeof(sock_name.sin_zero));

	// Extract port from address
	unsigned port = 80; // server port
	size_t colon_pos = address.find(':');
	// Colon has been found
	if (colon_pos < address.size()) {
		if (strtou(address, &port, colon_pos + 1) !=
			static_cast<int>(address.size() - colon_pos - 1))
		{
			errlog(config_path, ": incorrect port number");
			exit(7);
		}
		address[colon_pos] = '\0'; // need to extract IPv4 address
	} else
		address += '\0'; // need to extract IPv4 address
	// Set server port
	sock_name.sin_port = htons(port);
	address = "127.1.1.1"; // TODO: uncomment it

	// Extract IPv4 address
	if (0 == strcmp(address.data(), "*")) // strcmp because of '\0' in address
		sock_name.sin_addr.s_addr = htonl(INADDR_ANY); // server address
	else if (address.empty() ||
		inet_aton(address.data(), &sock_name.sin_addr) == 0)
	{
		errlog(config_path, ": incorrect IPv4 address");
		exit(8);
	}

	stdlog("ADDRESS: ", address, ':', toStr(port));
	stdlog("workers: ", toStr(workers));
	stdlog("connections: ", toStr(connections));
}

int main() {
	// Init server
	// Change directory to process executable directory
	try {
		chdirToExecDir();
	} catch (const std::exception& e) {
		errlog("Failed to change working directory: ", e.what());
	}

	// Loggers TODO: use global log filenames
	// Set stderr to write to server.log (stdlog writes to stderr)
	if (freopen("server2.log", "a", stderr) == NULL) // TODO: remove '2'
		errlog("Failed to open 'server.log'", error(errno));

	try {
		errlog.open("server2_error.log"); // TODO: remove '2'
	} catch (const std::exception& e) {
		errlog("Failed to open 'server_error.log': ", e.what());
	}

	// Signal control
	// TODO: manage server via signals
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &exit;

	(void)sigaction(SIGINT, &sa, nullptr);
	(void)sigaction(SIGTERM, &sa, nullptr);
	(void)sigaction(SIGQUIT, &sa, nullptr);

	stdlog("Initializing server...");
	stdlog.label(false); // Turn label off (temporarily)
	stdlog("PID: ", toStr(getpid()));

	// Load config
	sockaddr_in name;
	loadServerConfig("server.conf", name);

	socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
	if (socket_fd < 0) {
		errlog("Failed to create socket", error(errno));
		return 1;
	}

	int _true = 1;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &_true, sizeof(int))) {
		errlog("Error: setopt()", error(errno));
		return 2;
	}

	// Bind
	constexpr int TRIES = 8;
	for (int try_no = 1; bind(socket_fd, (sockaddr*)&name, sizeof(name));) {
		errlog("Failed to bind (try ", toStr(try_no), ')', error(errno));
		if (++try_no > TRIES)
			return 3;

		usleep(800000);
	}

	if (listen(socket_fd, 10)) {
		errlog("Error: listen()", error(errno));
		return 4;
	}

	try {
		master_process_cycle();

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return 5;

	} catch (...) {
		ERRLOG_CATCH();
		return 5;
	}

	return 0;
}
