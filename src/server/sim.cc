#include "sim_contest.h"
#include "sim_session.h"
#include "sim_template.h"
#include "sim_user.h"

#include <cppconn/prepared_statement.h>
#include <simlib/debug.h>
#include <simlib/filesystem.h>
#include <simlib/logger.h>
#include <simlib/time.h>

using std::string;
using std::unique_ptr;
using std::vector;

Sim::Sim() : db_conn(DB::createConnectionUsingPassFile(".db.config")),
		client_ip_(), req_(nullptr), resp_(server::HttpResponse::TEXT),
		contest(nullptr), session(nullptr), user(nullptr) {
	// Because of exception safety (we do not want to make a memory leak)
	try {
		contest = new Contest(*this);
		session = new Session(*this);
		user = new User(*this);

	} catch (...) {
		// Clean up
		delete contest;
		delete session;
		delete user;
		throw;
	}
}

Sim::~Sim() {
	delete contest;
	delete session;
	delete user;
}

server::HttpResponse Sim::handle(string client_ip,
		const server::HttpRequest& req) {
	client_ip_.swap(client_ip);
	req_ = &req;
	resp_ = server::HttpResponse(server::HttpResponse::TEXT);

	stdlog(req.target);

	try {
		if (0 == compareTo(req.target, 1, '/', "kit"))
			getStaticFile();

		else if (0 == compareTo(req.target, 1, '/', "logs"))
			logs();

		else if (0 == compareTo(req.target, 1, '/', "login"))
			user->login();

		else if (0 == compareTo(req.target, 1, '/', "logout"))
			user->logout();

		else if (0 == compareTo(req.target, 1, '/', "signup"))
			user->signUp();

		else if (0 == compareTo(req.target, 1, '/', "u"))
			user->handle();

		else if (0 == compareTo(req.target, 1, '/', "c"))
			contest->handle();

		else if (0 == compareTo(req.target, 1, '/', "s"))
			contest->submission();

		else if (0 == compareTo(req.target, 1, '/', ""))
			mainPage();

		else
			error404();

	} catch (const std::exception& e) {
		errlog("Caught exception: ", __FILE__, ':', toString(__LINE__),
			" -> ", e.what());
		error500();

	} catch (...) {
		errlog("Caught exception: ", __FILE__, ':', toString(__LINE__));
		error500();
	}

	// Make sure that session is closed
	session->close();
	return resp_;
}

void Sim::mainPage() {
	Template templ(*this, "Main page");
	templ << "<div style=\"text-align: center\">\n"
				"<img src=\"/kit/img/SIM-logo.png\" width=\"260\" height=\"336\" alt=\"\">\n"
				"<p style=\"font-size: 30px\">Welcome to SIM</p>\n"
				"<hr>\n"
				"<p>SIM is open source platform for carrying out algorithmic contests</p>\n"
			"</div>\n";
}

void Sim::getStaticFile() {
	string file = "public";
	// Extract path (ignore query)
	file += abspath(decodeURI(req_->target, 1, req_->target.find('?')));
	D(stdlog(file);)

	// Get file stat
	struct stat attr;
	if (stat(file.c_str(), &attr) != -1) {
		// Extract time of last modification
		resp_.headers["Last-Modified"] = date("%a, %d %b %Y %H:%M:%S GMT", attr.st_mtime);
		server::HttpHeaders::const_iterator it = req_->headers.find("if-modified-since");
		struct tm client_mtime;

		// If "If-Modified-Since" header is set and its value is not lower than
		// attr.st_mtime
		if (it != req_->headers.end() && strptime(it->second.c_str(),
				"%a, %d %b %Y %H:%M:%S GMT", &client_mtime) != nullptr &&
				timegm(&client_mtime) >= attr.st_mtime) {
			resp_.status_code = "304 Not Modified";
			resp_.content_type = server::HttpResponse::TEXT;
			return;
		}
	}

	resp_.content_type = server::HttpResponse::FILE;
	resp_.content = file;
}

// Cuts string to a newline character both from beginning and ending
inline static void cutToNewline(string& str) noexcept {
	// Suffix
	str.erase(std::min(str.size(), str.rfind('\n')));
	// Prefix
	str.erase(0, str.find('\n'));
}

static string colour(const string& str) noexcept {
	string res;
	enum : uint8_t { SPAN, B, NONE } opened = NONE;
	auto closeLastTag = [&] {
		switch (opened) {
			case SPAN: res += "</span>"; break;
			case B: res += "</b>"; break;
			case NONE: break;
		}
	};

	for (size_t i = 0; i < str.size(); ++i) {
		if (str.compare(i, 5, "\033[31m") == 0) {
			closeLastTag();
			res += "<span class=\"red\">";
			opened = SPAN;
			i += 4;
		} else if (str.compare(i, 5, "\033[32m") == 0) {
			closeLastTag();
			res += "<span class=\"green\">";
			opened = SPAN;
			i += 4;
		} else if (str.compare(i, 5, "\033[33m") == 0) {
			closeLastTag();
			res += "<span class=\"yellow\">";
			opened = SPAN;
			i += 4;
		} else if (str.compare(i, 5, "\033[34m") == 0) {
			closeLastTag();
			res += "<span class=\"blue\">";
			opened = SPAN;
			i += 4;
		} else if (str.compare(i, 7, "\033[1;31m") == 0) {
			closeLastTag();
			res += "<b class=\"red\">";
			opened = B;
			i += 6;
		} else if (str.compare(i, 7, "\033[1;32m") == 0) {
			closeLastTag();
			res += "<b class=\"green\">";
			opened = B;
			i += 6;
		} else if (str.compare(i, 7, "\033[1;33m") == 0) {
			closeLastTag();
			res += "<b class=\"yellow\">";
			opened = B;
			i += 6;
		} else if (str.compare(i, 7, "\033[1;34m") == 0) {
			closeLastTag();
			res += "<b class=\"blue\">";
			opened = B;
			i += 6;
		} else if (str.compare(i, 3, "\033[m") == 0) {
			closeLastTag();
			opened = NONE;
			i += 2;
		} else
			res += str[i];
	}
	closeLastTag();
	return res;
}

void Sim::logs() {
	if (session->open() != Session::OK || session->user_type > 0)
		return error403();

	Template templ(*this, "Logs");
	templ << "<pre class=\"logs\">";

	FileDescriptor fd;
	constexpr int BYTES_TO_READ = 65536;

	// server_error.log
	if (fd.open("server_error.log", O_RDONLY | O_LARGEFILE) == -1) {
		errlog(__PRETTY_FUNCTION__, ": open()", error(errno));
	} else {
		string contents = getFileContents(fd, -BYTES_TO_READ, -1);
		cutToNewline(contents);
		contents = colour(contents);
		templ << contents;
	}
	// TODO: more logs and show less, but make show more button

	templ << "</pre>";
}

void Sim::redirect(const string& location) {
	resp_.status_code = "302 Moved Temporarily";
	resp_.headers["Location"] = location;
}
