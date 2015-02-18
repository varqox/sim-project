#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <string>
#include <map>
#include <iostream>
#include <algorithm>

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

#ifdef DEBUG
# define D(...) __VA_ARGS__
#else
# define D(...)
#endif

using namespace std;

string myto_string(long long a) {
	string w;
	bool minus = false;
	if (a < 0) {
		minus = true;
		a = -a;
	}
	while (a > 0) {
		w += static_cast<char>(a % 10 + '0');
		a /= 10;
	}
	if (minus)
		w += "-";
	if (w.empty())
		w = "0";
	else
		std::reverse(w.begin(), w.end());
return w;
}

string myto_string(size_t a) {
	string w;
	while (a > 0) {
		w += static_cast<char>(a % 10 + '0');
		a /= 10;
	}
	if (w.empty())
		w = "0";
	else
		std::reverse(w.begin(), w.end());
return w;
}

inline int hextodec(char c) {
	c = tolower(c);
	return (c > 'a' ? 10 + c - 'a' : c - '0');
}

inline char dectohex(int x) { return x > 9 ? 'A' + x - 10 : x + '0'; }

string encodeURI(const string& str, size_t beg = 0, size_t end = string::npos) {
	// a-z A-Z 0-9 - _ . ~
	static bool is_safe[256] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
		0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1};
	if (end == string::npos)
		end = str.size();
	string ret;
	for (; beg < end; ++beg) {
		unsigned char c = str[beg];
		if (is_safe[c])
			ret += c;
		else {
			ret += '%';
			ret += dectohex(c >> 4);
			ret += dectohex(c & 15);
		}
	}
	return ret;
}

inline bool isPrefix(const std::string& str, const std::string& prefix) {
	return str.compare(0, prefix.size(), prefix) == 0;
}

string decodeURI(const string& str, size_t beg = 0, size_t end = string::npos) {
	if (end == string::npos)
		end = str.size();
	string ret;
	for (; beg < end; ++beg) {
		if (str[beg] == '%' && beg + 2 < end)
			ret += static_cast<char>((hextodec(str[beg+1]) << 4) + hextodec(str[beg+2])), beg += 2;
		else if (str[beg] == '+')
			ret += ' ';
		else
			ret += str[beg];
	}
	return ret;
}

void wrt(int fd, const char* str) {
	write(fd, str, strlen(str));
}

string tolower(string str) {
	for (size_t i = 0, s = str.size(); i < s; ++i)
		str[i] = tolower(str[i]);
	return str;
}

size_t strtosize_t(const string& s) {
	size_t res = 0;
	for (size_t i = 0; i < s.size(); ++i)
		res = res * 10 + s[i] - '0';
	return res;
}

template<class T, class K>
ostream& operator<<(ostream& os, const map<T, K>& x) {
	os << "{";
	if (x.size()) {
		os << x.begin()->first << " => " << x.begin()->second;
		for (typename map<T, K>::const_iterator i = ++x.begin(); i != x.end(); ++i)
			os << ",\n" << i->first << " => " << i->second;
	}
	return os << "}";
}

class Connection {
public:
	enum State { OK, CLOSED };

	class HeadersClass : public map<string, string> {
	public:
		string& operator[](const string& n) {
			return map<string, string>::operator[](n);
		}
	};

	class Request {
	public:
		enum Method { GET, POST, HEAD } method;
		HeadersClass headers;
		string target, http_version, content;

		struct Form {
			// files: name (this from form) => tmp_filename
			// other: name => value; if field is file, value = client filename
			map<string, string> files, other;

			operator map<string, string>&() {
				return other;
			}
			string& operator[](const string& key) { return other[key]; }

			~Form() {
				for (map<string, string>::iterator i = files.begin(); i != files.end(); ++i)
					unlink(i->second.c_str());
			}
		} form_data;
	};

	class Response {
	public:
		enum ContentType { TEXT, FILE } content_type;
		HeadersClass headers, cookies;
		string content;

		Response(ContentType con_type = TEXT) : content_type(con_type),
				headers(), content() {}

		void setCookie(const string& name, const string& val,
				time_t expire = -1, const string& path = "", const string& domain ="",
				bool http_only = false, bool secure = false) {
			string value = val;
			if (expire != -1) {
				char buff[35];
				tm *ptm = gmtime(&expire);
				if (strftime(buff, 35, "%a, %d %b %Y %H:%M:%S GMT", ptm))
					value.append("; Expires=").append(buff);
			}
			if (!path.empty())
				value.append("; Path=").append(path);
			if (!domain.empty())
				value.append("; Domain=").append(domain);
			if (http_only)
				value.append("; HttpOnly");
			if (secure)
				value.append("; Secure");
			cookies[name] = value;
		}

		string getCookie(const string& name) {
			string &cookie = cookies[name];
			return cookie.substr(0, cookie.find(';'));
		}
	};

private:
	static const size_t BUFFER_SIZE = 1 << 16;
	static const int POLL_TIMEOUT = 20 * 1000 ; // in miliseconds
	static const size_t MAX_CONTENT_LENGTH = 10 * (1 << 20); // 10 Mb
	static const size_t MAX_HEADER_LENGTH = 8192;

	enum State state_;
	int sock_fd_, buff_size_, pos_;
	unsigned char buffer_[1 << 16];

	int peek() {
		if (state_ == CLOSED)
			return -1;
		if (pos_ >= buff_size_) {
			// wait for data
			pollfd pfd = {sock_fd_, POLLIN, 0};
			D(eprintf("peek(): polling... ");)
			if (poll(&pfd, 1, POLL_TIMEOUT) <= 0) {
				D(eprintf("peek(): No response\n");)
				error408();
				return -1;
			}
			D(eprintf("peek(): OK\n");)
			pos_ = 0;
			buff_size_ = read(sock_fd_, buffer_, BUFFER_SIZE);
			D(eprintf("peek(): Reading completed; buff_size: %i\n", buff_size_);)
			if (buff_size_ <= 0) {
				state_ = CLOSED;
				return -1; // Failed
			}
		}
		return buffer_[pos_];
	}

	int getChar() {
		int res = peek();
		++pos_;
		return res;
	}

	class LimitedReader {
		Connection& conn_;
		size_t read_limit_;

	public:
		LimitedReader(Connection& cn, size_t rl): conn_(cn), read_limit_(rl) {}

		size_t limit() const { return read_limit_; }

		int getChar() {
			if (read_limit_ == 0)
				return -1;
			--read_limit_;
			return conn_.getChar();
		}
	};

	string getHeaderLine() {
		string line;
		int c;
		while ((c = getChar()) != -1) {
			if (c == '\n' && *--line.end() == '\r') {
				line.erase(--line.end());
				break;
			} else if (line.size() > MAX_HEADER_LENGTH) {
				error431();
				return "";
			} else
				line += c;
		}
		return (state_ == OK ? line : "");
	}

	pair<string, string> parseHeaderline(const string& header) {
		size_t beg = header.find(':'), end;
		if (beg == string::npos) {
			error400();
			return pair<string, string>();
		}
		// Check for whitespace in field-name
		end = header.find(' ');
		if (end != string::npos && end < beg) {
			error400();
			return pair<string, string>();
		}
		string ret = header.substr(0, beg);
		// Erase leading whitespace
		end = header.size();
		while (isspace(header[end - 1]))
			--end;
		// Erase trailing whitespace
		while (++beg < header.size() && isspace(header[beg])) {}
		return make_pair(ret, header.substr(beg, end - beg));
	}

	void readPOST(Request& req) {
		size_t content_length = strtosize_t(req.headers["Content-Length"]);

		int c = '\0';
		string field_name, field_content;
		bool is_name;
		string &con_type = req.headers["Content-Type"];

		LimitedReader reader(*this, content_length);

		if (isPrefix(con_type, "text/plain")) {
			for (; c != -1;) {
				// Clear all variables
				field_name = field_content = "";
				is_name = true;
				while ((c = reader.getChar()) != -1) {
					if (c == '\r') {
						if (peek() == '\n')
							c = reader.getChar();
						break;
					}
					if (is_name && c == '=')
						is_name = false;
					else if (is_name)
						field_name += c;
					else
						field_content += c;
				}
				if (state_ == CLOSED)
					return;
				req.form_data[field_name] = field_content;
			}
		} else if (isPrefix(con_type, "application/x-www-form-urlencoded")) {
			for (; c != -1;) {
				// Clear all variables
				field_name = field_content = "";
				is_name = true;
				while ((c = reader.getChar()) != -1) {
					if (c == '&')
						break;
					if (is_name && c == '=')
						is_name = false;
					else if (is_name)
						field_name += c;
					else
						field_content += c;
				}
				if (state_ == CLOSED)
					return;
				req.form_data[decodeURI(field_name)] = decodeURI(field_content);
			}
		} else if (isPrefix(con_type, "multipart/form-data")) {
			size_t beg = con_type.find("boundary=");
			if (beg == string::npos || beg + 9 >= con_type.size()) {
				error400();
				return;
			}
			string boundary = "\r\n--"; // this is always in request expect beginning
			boundary += con_type.substr(beg + 9);
			// Compute p array for KMP algorithm
			int p[boundary.size()];
			size_t k = 0;
			p[0] = 0;
			for (size_t i = 1; i < boundary.size(); ++i) {
				while (k > 0 && boundary[k] != boundary[i])
					k = p[k - 1];
				if (boundary[k] == boundary[i])
					++k;
				p[i] = k;
			}
			// Search for boundary
			int fd = -1;
			FILE *tmp_file = NULL;
			bool first_boundary = true;
			k = 2; // because "\r\n" don't have to exist at beginning

			// While we can read
			// In each loop pass parse EXACTLY one field
			while ((c = reader.getChar()) != -1) {
				while (k > 0 && boundary[k] != c)
					k = p[k - 1];

				if (boundary[k] == c)
					++k;

				// If we have found boundary
				if (k == boundary.size()) {
					if (first_boundary)
						first_boundary = false;
					else {
						// Manage last field
						if (fd == -1) {
							// This was normal variable
							// Guarantee that pos is >= 0
							// +1 because we didn't append last character to boundary
							field_content.erase((field_content.size() < boundary.size()
									? 0 : field_content.size() - boundary.size() + 1));
							req.form_data[field_name] = field_content;
						} else {
							// We had file
							fflush(tmp_file);

							fseek(tmp_file, 0, SEEK_END);
							size_t tmp_file_size = ftell(tmp_file);
							// Guarantee that length is >= 0
							// +1 because we didn't append last character to boundary
							ftruncate(fileno(tmp_file), (tmp_file_size < boundary.size()
									? 0 : tmp_file_size - boundary.size() + 1));

							fclose(tmp_file);
							fd = -1;
						}
					}

					// Prepare next field
					// Ignore LFCR or "--"
					if (reader.getChar() == -1 || reader.getChar() == -1) {
						error400();
						goto safe_return;
					}

					// Headers
					for (;;) {
						field_content = ""; // In this case header
						while ((c = reader.getChar()) != -1) {
							// Found CRLF
							if (c == '\n' && *--field_content.end() == '\r') {
								field_content.erase(--field_content.end());
								break;
							} else if (field_content.size() > MAX_HEADER_LENGTH) {
								error431();
								goto safe_return;
							} else
								field_content += c;
						}
						if (state_ != OK) // Something wnt wrong
							goto safe_return;
						if (field_content.empty()) // End of headers
							break;

						D(cerr << "header: '" << field_content << '\'' << endl;)

						pair<string, string> header = parseHeaderline(field_content);
						if (state_ != OK) // Something wnt wrong
							goto safe_return;

						if (tolower(header.first) == "content-disposition") {
							// extract all needed information
							size_t st = 0, last = 0;
							field_name = "";
							header.second += ';';
							string var_name, var_val;
							char tmp_filename[] = "/tmp/sim-server-tmp.XXXXXX";

							// extract all variables from header content
							while ((last = header.second.find(';', st)) != string::npos) {
								while (isblank(header.second[st]))
									++st;
								var_name = var_val = "";

								// extract var_name
								while (st < last && !isblank(header.second[st]) && header.second[st] != '=')
									var_name += header.second[st++];

								// extract var_val
								if (header.second[st] == '=') {
									++st;
									// this is safe because lst characer is always ';'
									if (header.second[st] == '"')
										while (++st < last && header.second[st] != '"') {
											if (header.second[st] == '\\')
												++st; // safe because last character is ';'
											var_val += header.second[st];
										}
									else
										while (st < last && !isblank(header.second[st]))
											var_val += header.second[st++];
								}
								st = last + 1;

								// Check for specific values
								if (var_name == "filename" && fd == -1) {
									fd = mkstemp(tmp_filename);
									if (fd == -1) {
										error507();
										goto safe_return;
									}
									field_content = var_val; // store client filename
								} else if (var_name == "name")
									field_name = var_val;
							}

							// Add field to req.form_data
							if (fd != -1) {
								tmp_file = fdopen(fd, "w");
								req.form_data.files[field_name] = tmp_filename;
								req.form_data[field_name] = field_content; // field_name => client filename
							}
						}
					}
					k = p[k - 1];
				} else if (fd == -1) {
					field_content += c;
					if (field_content.size() > MAX_CONTENT_LENGTH) {
						error413();
						goto safe_return;
					}
				} else
					putc(c, tmp_file);
			}
		safe_return:
			// Remove trash
			if (fd != -1)
				fclose(tmp_file);
		} else
			error415();
	}

public:
	Connection(int client_socket_fd): state_(OK), sock_fd_(client_socket_fd),
			buff_size_(0), pos_(0) {}
	~Connection() {}

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

	void error400() {
		send("<html>\n"
			"<head><title>400 Bad Request</title></head>\n"
			"<body>\n"
			"<center><h1>400 Bad Request</h1></center>\n"
			"</body>\n"
			"</html>\n");
		state_ = CLOSED;
	}

	void error408() {
		send("HTTP/1.1 408 Request Timeout\r\n"
			"Server: sim-server\r\n"
			"Connection: close\r\n"
			"Content-Type: text/html; charset=utf-8\r\n"
			"Content-Length: 128\r\n"
			"\r\n"
			"<html>\n"
			"<head><title>408 Request Timeout</title></head>\n"
			"<body>\n"
			"<center><h1>408 Request Timeout</h1></center>\n"
			"</body>\n"
			"</html>\n");
		state_ = CLOSED;
	}

	void error413() {
		send("HTTP/1.1 413 Request Entity Too Large\r\n"
			"Server: sim-server\r\n"
			"Connection: close\r\n"
			"Content-Type: text/html; charset=utf-8\r\n"
			"Content-Length: 146\r\n"
			"\r\n"
			"<html>\n"
			"<head><title>413 Request Entity Too Large</title></head>\n"
			"<body>\n"
			"<center><h1>413 Request Entity Too Large</h1></center>\n"
			"</body>\n"
			"</html>\n");
		state_ = CLOSED;
	}

	void error415() {
		send("HTTP/1.1 415 Unsupported Media Type\r\n"
			"Server: sim-server\r\n"
			"Connection: close\r\n"
			"Content-Type: text/html; charset=utf-8\r\n"
			"Content-Length: 142\r\n"
			"\r\n"
			"<html>\n"
			"<head><title>415 Unsupported Media Type</title></head>\n"
			"<body>\n"
			"<center><h1>415 Unsupported Media Type</h1></center>\n"
			"</body>\n"
			"</html>\n");
		state_ = CLOSED;
	}

	void error431() {
		send("HTTP/1.1 431 Request Header Fields Too Large\r\n"
			"Server: sim-server\r\n"
			"Connection: close\r\n"
			"Content-Type: text/html; charset=utf-8\r\n"
			"Content-Length: 160\r\n"
			"\r\n"
			"<html>\n"
			"<head><title>431 Request Header Fields Too Large</title></head>\n"
			"<body>\n"
			"<center><h1>431 Request Header Fields Too Large</h1></center>\n"
			"</body>\n"
			"</html>\n");
		state_ = CLOSED;
	}

	void error500() {
		send("HTTP/1.1 500 Internal Server Error\r\n"
			"Server: sim-server\r\n"
			"Connection: close\r\n"
			"Content-Type: text/html; charset=utf-8\r\n"
			"Content-Length: 136\r\n"
			"\r\n"
			"<html>\n"
			"<head><title>500 Internal Server Error</title></head>\n"
			"<body>\n"
			"<center><h1>500 Internal Server Error</h1></center>\n"
			"</body>\n"
			"</html>\n");
		state_ = CLOSED;
	}

	void error501() {
		send("HTTP/1.1 501 Not Implemented\r\n"
			"Server: sim-server\r\n"
			"Connection: close\r\n"
			"Content-Type: text/html; charset=utf-8\r\n"
			"Content-Length: 124\r\n"
			"\r\n"
			"<html>\n"
			"<head><title>501 Not Implemented</title></head>\n"
			"<body>\n"
			"<center><h1>501 Not Implemented</h1></center>\n"
			"</body>\n"
			"</html>\n");
		state_ = CLOSED;
	}

	void error504() {
		send("HTTP/1.1 504 Gateway Timeout\r\n"
			"Server: sim-server\r\n"
			"Connection: close\r\n"
			"Content-Type: text/html; charset=utf-8\r\n"
			"Content-Length: 124\r\n"
			"\r\n"
			"<html>\n"
			"<head><title>504 Gateway Timeout</title></head>\n"
			"<body>\n"
			"<center><h1>504 Gateway Timeout</h1></center>\n"
			"</body>\n"
			"</html>\n");
		state_ = CLOSED;
	}

	void error507() {
		send("HTTP/1.1 507 Insufficient Storage\r\n"
			"Server: sim-server\r\n"
			"Connection: close\r\n"
			"Content-Type: text/html; charset=utf-8\r\n"
			"Content-Length: 134\r\n"
			"\r\n"
			"<html>\n"
			"<head><title>507 Insufficient Storage</title></head>\n"
			"<body>\n"
			"<center><h1>507 Insufficient Storage</h1></center>\n"
			"</body>\n"
			"</html>\n");
		state_ = CLOSED;
	}

	Request getRequest() {
		Request req;

		// Get request line
		string request_line = getHeaderLine(), tmp;
		while (state_ == OK && request_line.empty())
			request_line = getHeaderLine();
		if (state_ == CLOSED)
			return req;
		D(cerr << "\nREQUEST: " << request_line << endl;)

		// Extract method
		size_t beg = 0, end = 0;
		while (end < request_line.size() && !isspace(request_line[end]))
			++end;
		if (request_line.compare(0, end, "GET") == 0)
			req.method = Request::GET;
		else if (request_line.compare(0, end, "POST") == 0)
			req.method = Request::POST;
		else if (request_line.compare(0, end, "HEAD") == 0)
			req.method = Request::HEAD;
		else {
			error400();
			return req;
		}

		// Exract target
		while (end < request_line.size() && isspace(request_line[end]))
			++end;
		beg = end;
		while (end < request_line.size() && !isspace(request_line[end]))
			++end;
		req.target = request_line.substr(beg, end - beg);
		if (req.target.compare(0, 1, "/") != 0) {
			error400();
			return req;
		}

		// Extract http version
		while (end < request_line.size() && isspace(request_line[end]))
			++end;
		beg = end;
		while (end < request_line.size() && !isspace(request_line[end]))
			++end;
		req.http_version = request_line.substr(beg, end - beg);
		if (req.http_version.compare(0, 7, "HTTP/1.") != 0 ||
			(req.http_version.compare(7, string::npos, "0") != 0 &&
			req.http_version.compare(7, string::npos, "1") != 0)) {
			error400();
			return req;
		}

		// Read headers
		req.headers["Content-Length"] = "0";
		string header;
		D(cerr << "HEADERS: \n";)
		while ((header = getHeaderLine()).size()) {
			D(cerr << '\t' << header << endl;)
			pair<string, string> hdr = parseHeaderline(header);
			if (state_ == CLOSED)
				return req;
			req.headers[hdr.first] = hdr.second;
		}
		D(cerr << "\n";)
		if (state_ == CLOSED)
			return req;

		// Read content
		if (req.method == Request::POST) {
			readPOST(req);
			return req;
		}

		end = strtosize_t(req.headers["Content-Length"]);
		if (end > 0) {
			if (end > MAX_CONTENT_LENGTH) {
				error413();
				return req;
			}

			char *s = new char[end];
			if (s == NULL) {
				error507();
				return req;
			}

			beg = -1;

			while (++beg < end) {
				int c = getChar();
				if (c == -1) {
					delete[] s;
					return req;
				}
				s[beg] = c;
			}
			req.content = s;
			delete[] s;
		}
		return req;
	}

	void send(const char* str, size_t len) {
		if (state_ == CLOSED)
			return;
		size_t pos = 0;
		ssize_t written;
		while (pos < len) {
			written = write(sock_fd_, str + pos, len - pos);
			cerr << "written: " << written << endl;
			if (written == -1) {
				state_ = CLOSED;
				break;
			}
			pos += written;
		}
	}

	void send(const string& str) {
		send(str.c_str(), str.size());
	}

	void sendResponse(const Response& res) {
		string str;
		if (res.headers.find("location") != res.headers.end())
			str += "HTTP/1.1 302 Moved Temporarily\r\n";
		else
			str += "HTTP/1.1 200 OK\r\n";
		str += "Server: sim-server\r\n";
		str += "Connection: close\r\n";
		for (HeadersClass::const_iterator i = res.headers.begin(); i != res.headers.end(); ++i) {
			if (i->first == "server" || i->first == "connection" || i->first == "content-length")
				continue;
			str += i->first;
			str += ": ";
			str += i->second;
			str += "\r\n";
		}
		for (HeadersClass::const_iterator i = res.cookies.begin(); i != res.cookies.end(); ++i) {
			str += "Set-Cookie: ";
			str += i->first;
			str += "=";
			str += i->second;
			str += "\r\n";
		}
		if (res.content_type == Response::TEXT) {
			str += "Content-Length: ";
			str += myto_string(res.content.size());
			str += "\r\n\r\n";
			str += res.content;
			send(str);
		} else { // content _type == FILE
			int fd = open(res.content.c_str(), O_RDONLY);
			if (fd == -1) {
				error500();
				return;
			}

			off_t fsize = lseek(fd, 0, SEEK_END);
			if (fsize == (off_t)-1 || lseek(fd, 0, SEEK_SET) == (off_t)-1) {
				error500();
				return;
			}

			str += "Accept-Ranges: bytes\r\n";
			str += "Content-Length: ";
			str += myto_string((size_t)fsize);
			str += "\r\n\r\n";

			const size_t buff_length = 1 << 20;
			char *buff = new char[buff_length];
			if (buff == NULL) {
				error507();
				return;
			}
			send(str);
			if (state_ == CLOSED) {
				delete[] buff;
				return;
			}

			// Read from file and write to socket
			off_t pos = 0;
			ssize_t read_len;
			while (pos < fsize && state_ == OK && (read_len = read(fd, buff, buff_length)) != -1) {
				send(buff, read_len);
				pos += read_len;
			}

			delete[] buff;
			close(fd);
		}
		state_ = CLOSED;
	}
};

int socket_fd;

void* worker(void*) {
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
			Connection::Request req = conn.getRequest();
			eprintf("\nContent: '%s'\n", req.content.c_str());
			if (req.method == Connection::Request::POST) {
				cerr << "form_data: " << req.form_data.other << endl;
				cerr << "form_data -> files: " << req.form_data.files << endl;
			}
			if (conn.state() == Connection::OK) {
				Connection::Response resp;
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

#define WORKERS 3

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
		pthread_create(threads + i, NULL, worker, NULL);
	threads[0] = pthread_self();
	worker(NULL);
	close(socket_fd);
	return 0;
}
