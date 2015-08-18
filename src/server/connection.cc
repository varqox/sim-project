#include "connection.h"

#include "../simlib/include/debug.h"

#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>

using std::string;
using std::pair;
using std::cerr;
using std::endl;

namespace server {

int Connection::peek() {
	if (state_ == CLOSED)
		return -1;

	if (pos_ >= buff_size_) {
		// wait for data
		pollfd pfd = {sock_fd_, POLLIN, 0};
		E("peek(): polling... ");

		if (poll(&pfd, 1, POLL_TIMEOUT) <= 0) {
			E("peek(): No response\n");
			error408();
			return -1;
		}
		E("peek(): OK\n");

		pos_ = 0;
		buff_size_ = read(sock_fd_, buffer_, BUFFER_SIZE);
		E("peek(): Reading completed; buff_size: %i\n", buff_size_);

		if (buff_size_ <= 0) {
			state_ = CLOSED;
			return -1; // Failed
		}
	}

	return buffer_[pos_];
}

string Connection::getHeaderLine() {
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

pair<string, string> Connection::parseHeaderline(const string& header) {
	size_t beg = header.find(':'), end;
	if (beg == string::npos) {
		error400();
		return pair<string, string>();
	}

	// Check for white space in field-name
	end = header.find(' ');
	if (end != string::npos && end < beg) {
		error400();
		return pair<string, string>();
	}

	string ret = header.substr(0, beg);
	// Erase leading white space
	end = header.size();
	while (isspace(header[end - 1]))
		--end;

	// Erase trailing white space
	while (++beg < header.size() && isspace(header[beg])) {}

	return make_pair(ret, header.substr(beg, end - beg));
}

void Connection::readPOST(HttpRequest& req) {
	size_t content_length = strtoull(req.headers["Content-Length"]);
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

		string boundary = "\r\n--"; // Is always part of a request, except at
		                            // the beginning
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
		k = 2; // Because "\r\n" may not exist at the beginning

		// TODO: consider changing structure of while below
		// While we can read
		// In each loop pass parse EXACTLY one field
		while ((c = reader.getChar()) != -1) {
			while (k > 0 && boundary[k] != c)
				k = p[k - 1];

			if (boundary[k] == c)
				++k;
			// If we have found a boundary
			if (k == boundary.size()) {
				if (first_boundary)
					first_boundary = false;

				// Manage last field
				else {
					// Normal variable
					if (fd == -1) {
						// Erase boundary: +1 because we did not append the last
						// character to the boundary
						field_content.erase(
							(field_content.size() < boundary.size() ? 0 :
								field_content.size() - boundary.size() + 1));
						req.form_data[field_name] = field_content;

					// File
					} else {
						// Get file size
						fflush(tmp_file);
						fseek(tmp_file, 0, SEEK_END);
						size_t tmp_file_size = ftell(tmp_file);
						// Erase boundary: +1 because we did not append the last
						// character to the boundary
						ftruncate(fileno(tmp_file),
							(tmp_file_size < boundary.size() ? 0 :
								tmp_file_size - boundary.size() + 1));

						fclose(tmp_file);
						fd = -1;
					}
				}

				// Prepare next field
				// Ignore LFCR or "--"
				reader.getChar();
				if (reader.getChar() == -1) {
					error400();
					goto safe_return;
				}

				// Headers
				for (;;) {
					field_content = ""; // Header in this case

					while ((c = reader.getChar()) != -1) {
						// Found CRLF
						if (c == '\n' && *--field_content.end() == '\r') {
							field_content.erase(--field_content.end());
							break;

						} else if (field_content.size() > MAX_HEADER_LENGTH) {
							error431();
							goto safe_return;
						}

						field_content += c;
					}

					if (state_ != OK) // Something went wrong
						goto safe_return;

					if (field_content.empty()) // End of headers
						break;

					E("header: '%s'\n", field_content.c_str());
					pair<string, string> header =
						parseHeaderline(field_content);
					if (state_ != OK) // Something went wrong
						goto safe_return;

					if (tolower(header.first) == "content-disposition") {
						// extract all needed information
						size_t st = 0, last = 0;
						field_name = "";
						header.second += ';';
						string var_name, var_val;
						char tmp_filename[] = "/tmp/sim-server-tmp.XXXXXX";

						// extract all variables from header content
						while ((last = header.second.find(';', st)) !=
								string::npos) {
							while (isblank(header.second[st]))
								++st;

							var_name = var_val = "";
							// extract var_name
							while (st < last && !isblank(header.second[st]) &&
									header.second[st] != '=')
								var_name += header.second[st++];

							// extract var_val
							if (header.second[st] == '=') {
								++st; // this is safe because last character is
								      // always ';'

								if (header.second[st] == '"')
									while (++st < last &&
											header.second[st] != '"') {
										if (header.second[st] == '\\')
											++st; // safe because last character
												  // is ';'
										var_val += header.second[st];
									}

								else
									while (st < last &&
											!isblank(header.second[st]))
										var_val += header.second[st++];
							}
							st = last + 1;

							// Check for specific values
							if (var_name == "filename" && fd == -1) {
								if ((fd = mkstemp(tmp_filename)) == -1) {
									error507();
									goto safe_return;
								}

								// store client filename
								field_content = var_val;

							} else if (var_name == "name")
								field_name = var_val;
						}

						// Add file field to req.form_data
						if (fd != -1) {
							tmp_file = fdopen(fd, "w");
							req.form_data.files[field_name] = tmp_filename;
							// field_name => client filename
							req.form_data[field_name] = field_content;
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

void Connection::error400() {
	send("<html>\n"
		"<head><title>400 Bad Request</title></head>\n"
		"<body>\n"
		"<center><h1>400 Bad Request</h1></center>\n"
		"</body>\n"
		"</html>\n");
	state_ = CLOSED;
}

void Connection::error408() {
	send("HTTP/1.1 408 Request Timeout\r\n"
		"Server: sim-server\r\n"
		"Connection: close\r\n"
		"Content-Type: text/html; charset=utf-8\r\n"
		"Content-Length: 124\r\n"
		"\r\n"
		"<html>\n"
		"<head><title>408 Request Timeout</title></head>\n"
		"<body>\n"
		"<center><h1>408 Request Timeout</h1></center>\n"
		"</body>\n"
		"</html>\n");
	state_ = CLOSED;
}

void Connection::error403() {
	send("HTTP/1.1 403 Forbidden\r\n"
		"Server: sim-server\r\n"
		"Connection: close\r\n"
		"Content-Type: text/html; charset=utf-8\r\n"
		"Content-Length: 112\r\n"
		"\r\n"
		"<html>\n"
		"<head><title>403 Forbidden</title></head>\n"
		"<body>\n"
		"<center><h1>403 Forbidden</h1></center>\n"
		"</body>\n"
		"</html>\n");
	state_ = CLOSED;
}

void Connection::error404() {
	send("HTTP/1.1 404 Not Found\r\n"
		"Server: sim-server\r\n"
		"Connection: close\r\n"
		"Content-Type: text/html; charset=utf-8\r\n"
		"Content-Length: 112\r\n"
		"\r\n"
		"<html>\n"
		"<head><title>404 Not Found</title></head>\n"
		"<body>\n"
		"<center><h1>404 Not Found</h1></center>\n"
		"</body>\n"
		"</html>\n");
	state_ = CLOSED;
}

void Connection::error413() {
	send("HTTP/1.1 413 Request Entity Too Large\r\n"
		"Server: sim-server\r\n"
		"Connection: close\r\n"
		"Content-Type: text/html; charset=utf-8\r\n"
		"Content-Length: 142\r\n"
		"\r\n"
		"<html>\n"
		"<head><title>413 Request Entity Too Large</title></head>\n"
		"<body>\n"
		"<center><h1>413 Request Entity Too Large</h1></center>\n"
		"</body>\n"
		"</html>\n");
	state_ = CLOSED;
}

void Connection::error415() {
	send("HTTP/1.1 415 Unsupported Media Type\r\n"
		"Server: sim-server\r\n"
		"Connection: close\r\n"
		"Content-Type: text/html; charset=utf-8\r\n"
		"Content-Length: 138\r\n"
		"\r\n"
		"<html>\n"
		"<head><title>415 Unsupported Media Type</title></head>\n"
		"<body>\n"
		"<center><h1>415 Unsupported Media Type</h1></center>\n"
		"</body>\n"
		"</html>\n");
	state_ = CLOSED;
}

void Connection::error431() {
	send("HTTP/1.1 431 Request Header Fields Too Large\r\n"
		"Server: sim-server\r\n"
		"Connection: close\r\n"
		"Content-Type: text/html; charset=utf-8\r\n"
		"Content-Length: 156\r\n"
		"\r\n"
		"<html>\n"
		"<head><title>431 Request Header Fields Too Large</title></head>\n"
		"<body>\n"
		"<center><h1>431 Request Header Fields Too Large</h1></center>\n"
		"</body>\n"
		"</html>\n");
	state_ = CLOSED;
}

void Connection::error500() {
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

void Connection::error501() {
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

void Connection::error504() {
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

void Connection::error507() {
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

HttpRequest Connection::getRequest() {
	HttpRequest req;

	// Get request line
	string request_line = getHeaderLine(), tmp;
	while (state_ == OK && request_line.empty())
		request_line = getHeaderLine();

	if (state_ == CLOSED)
		return req;

	E("\nREQUEST: %s\n", request_line.c_str());
	// Extract method
	size_t beg = 0, end = 0;

	while (end < request_line.size() && !isspace(request_line[end]))
		++end;

	if (request_line.compare(0, end, "GET") == 0)
		req.method = HttpRequest::GET;
	else if (request_line.compare(0, end, "POST") == 0)
		req.method = HttpRequest::POST;
	else if (request_line.compare(0, end, "HEAD") == 0)
		req.method = HttpRequest::HEAD;
	else {
		error400();
		return req;
	}

	// Extract target
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

	E("HEADERS: \n");
	while ((header = getHeaderLine()).size()) {
		E("\t%s\n", header.c_str());
		pair<string, string> hdr = parseHeaderline(header);

		if (state_ == CLOSED)
			return req;

		req.headers[hdr.first] = hdr.second;
	}
	E("\n");

	if (state_ == CLOSED)
		return req;

	// Read content
	if (req.method == HttpRequest::POST) {
		readPOST(req);
		return req;
	}

	end = strtoull(req.headers["Content-Length"]);
	if (end > 0) {
		if (end > MAX_CONTENT_LENGTH) {
			error413();
			return req;
		}

		char *s = (char*)malloc(end);
		if (s == NULL) {
			error507();
			return req;
		}

		beg = -1;
		while (++beg < end) {
			int c = getChar();
			if (c == -1) {
				free(s);
				return req;
			}

			s[beg] = c;
		}

		req.content = s;
		free(s);
	}

	return req;
}

void Connection::send(const char* str, size_t len) {
	if (state_ == CLOSED)
		return;

	size_t pos = 0;
	ssize_t written;

	while (pos < len) {
		written = write(sock_fd_, str + pos, len - pos);
		E("written: %zi\n", written);

		if (written == -1) {
			state_ = CLOSED;
			break;
		}

		pos += written;
	}
}

void Connection::sendResponse(const HttpResponse& res) {
	string str = "HTTP/1.1 ";
	str.append(res.status_code).append("\r\n");
	str += "Server: sim-server\r\n";
	str += "Connection: close\r\n";

	for (HttpHeaders::const_iterator i = res.headers.begin(),
			end = res.headers.end(); i != end; ++i) {
		if (i->first == "server" || i->first == "connection" ||
				i->first == "content-length")
			continue;

		str += i->first;
		str += ": ";
		str += i->second;
		str += "\r\n";
	}

	for (HttpHeaders::const_iterator i = res.cookies.begin(),
			end = res.cookies.end(); i != end; ++i) {
		str += "Set-Cookie: ";
		str += i->first;
		str += "=";
		str += i->second;
		str += "\r\n";
	}

	switch (res.content_type) {
	case HttpResponse::TEXT:
		str += "Content-Length: ";
		str += toString(res.content.size());
		str += "\r\n\r\n";
		str += res.content;
		send(str);
		break;

	case HttpResponse::FILE:
		int fd = open(res.content.c_str(), O_RDONLY | O_LARGEFILE);
		if (fd == -1) {
			error404();
			return;
		}

#ifdef __x86_64__
		struct stat sb;
		if (fstat(fd, &sb) == -1) {
			error500();
			return;
		}
#else
		struct stat64 sb;
		if (fstat64(fd, &sb) == -1) {
			error500();
			return;
		}
#endif

		if (!S_ISREG(sb.st_mode)) {
			error403();
			return;
		}

		off64_t fsize = sb.st_size;
		str += "Accept-Ranges: none\r\n"; // Not supported yet, change to: bytes
		str += "Content-Length: ";
		str += toString((size_t)fsize);
		str += "\r\n\r\n";

		const size_t buff_length = 1 << 20;
		char *buff = (char*)malloc(buff_length);
		if (buff == NULL) {
			error507();
			return;
		}

		send(str);
		if (state_ == CLOSED) {
			free(buff);
			return;
		}

		// Read from file and write to socket
		off64_t pos = 0;
		ssize_t read_len;
		while (pos < fsize && state_ == OK &&
				(read_len = read(fd, buff, buff_length)) != -1) {
			send(buff, read_len);
			pos += read_len;
		}

		free(buff);
		close(fd);
	}

	state_ = CLOSED;
}

} // namespace server
