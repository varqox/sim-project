#pragma once

#include "http_request.hh"
#include "http_response.hh"

#include <simlib/likely.hh>

namespace server {

class Connection {
private:
	static const size_t BUFFER_SIZE = 1 << 16;
	static const int POLL_TIMEOUT = 20 * 1000; // in milliseconds
	static const size_t MAX_CONTENT_LENGTH = 10 << 20; // 10 MiB
	static const size_t MAX_HEADER_LENGTH = 8192;

public:
	enum State : uint8_t { OK, CLOSED };

private:
	State state_;
	int sock_fd_, buff_size_, pos_;
	unsigned char buffer_[BUFFER_SIZE];

	int peek();

	int get_char() {
		int res = peek();
		++pos_;
		return res;
	}

	class LimitedReader {
		Connection& conn_;
		size_t read_limit_;

	public:
		LimitedReader(Connection& cn, size_t rl) : conn_(cn), read_limit_(rl) {}

		size_t limit() const { return read_limit_; }

		int get_char() {
			if (UNLIKELY(read_limit_ == 0))
				return -1;

			--read_limit_;
			return conn_.get_char();
		}
	};

	std::string get_header_line();
	std::pair<std::string, std::string>
	parse_header_line(const std::string& header);
	void read_post(HttpRequest& req);

public:
	explicit Connection(int client_socket_fd)
	   : state_(OK), sock_fd_(client_socket_fd), buff_size_(0), pos_(0) {}

	~Connection() = default;

	State state() const { return state_; }

	void clear() {
		state_ = OK;
		buff_size_ = 0;
		pos_ = 0;
	}

	void assign(int new_sock_fd) {
		sock_fd_ = new_sock_fd;
		clear();
	}

	void error400();
	void error403();
	void error404();
	void error408();
	void error413();
	void error415();
	void error431();
	void error500();
	void error501();
	void error504();
	void error507();

	HttpRequest get_request();
	void send(const char* str, size_t len);
	void send(const std::string& str) { send(str.c_str(), str.size()); }
	void send_response(const HttpResponse& res);
};

} // namespace server
