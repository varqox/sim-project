#include "sim.h"

#include <simlib/debug.h>
#include <simlib/logger.h>
#include <simlib/time.h>

using std::string;
using std::unique_ptr;
using std::vector;

server::HttpResponse Sim::handle(string _client_ip,
	const server::HttpRequest& request)
{
	client_ip = std::move(_client_ip);
	req = &request;
	resp = server::HttpResponse(server::HttpResponse::TEXT);

	stdlog(req->target);

	// TODO: this is pretty bad-looking
	auto hardError500 = [&] {
		resp.status_code = "500 Internal Server Error";
		resp.headers["Content-Type"] = "text/html; charset=utf-8";
		resp.content = "<!DOCTYPE html>"
				"<html lang=\"en\">"
				"<head><title>500 Internal Server Error</title></head>"
				"<body>"
					"<center>"
						"<h1>500 Internal Server Error</h1>"
						"<p>Try to reload the page in a few seconds.</p>"
						"<button onclick=\"history.go(0)\">Reload</button>"
						"</center>"
				"</body>"
			"</html>";
	};

	try {
		url_args = RequestURIParser {req->target};
		StringView next_arg = url_args.extractNextArg();
		Template::reset();

		if (next_arg == "kit")
			getStaticFile();

		else if (next_arg == "logs")
			logs();

		else if (next_arg == "login")
			login();

		else if (next_arg == "logout")
			logout();

		else if (next_arg == "signup")
			signUp();

		else if (next_arg == "u")
			User::handle();

		else if (next_arg == "c")
			Contest::handle();

		else if (next_arg == "s")
			submission();

		else if (next_arg == "file")
			file();

		else if (next_arg == "")
			mainPage();

		else
			error404();

		Template::endTemplate();

		// Make sure that the session is closed
		Session::close();

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		hardError500(); // We cannot use error500() because it may throw

	} catch (...) {
		ERRLOG_CATCH();
		hardError500(); // We cannot use error500() because it may throw
	}

	return std::move(resp);
}

void Sim::mainPage() {
	baseTemplate("Main page");
	append("<div style=\"text-align: center\">"
			"<img src=\"/kit/img/SIM-logo.png\" width=\"260\" height=\"336\" "
				"alt=\"\">"
			"<p style=\"font-size: 30px\">Welcome to SIM</p>"
			"<hr>"
			"<p>SIM is an open source platform for carrying out algorithmic "
				"contests</p>"
		"</div>");
}

void Sim::getStaticFile() {
	string file_path = "public";
	// Extract path (ignore query)
	file_path += abspath(decodeURI(req->target, 1, req->target.find('?')));
	D(stdlog(file_path);)

	// Get file stat
	struct stat attr;
	if (stat(file_path.c_str(), &attr) != -1) {
		// Extract time of last modification
		auto it = req->headers.find("if-modified-since");
		resp.headers["Last-Modified"] = date("%a, %d %b %Y %H:%M:%S GMT",
			attr.st_mtime);

		// If "If-Modified-Since" header is set and its value is not lower than
		// attr.st_mtime
		struct tm client_mtime;
		if (it != req->headers.end() && strptime(it->second.c_str(),
			"%a, %d %b %Y %H:%M:%S GMT", &client_mtime) != nullptr &&
			timegm(&client_mtime) >= attr.st_mtime)
		{
			resp.status_code = "304 Not Modified";
			return;
		}
	}

	resp.content_type = server::HttpResponse::FILE;
	resp.content = file_path;
}

static string colorize(const string& str) noexcept {
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
		} else if (str.compare(i, 5, "\033[35m") == 0) {
			closeLastTag();
			res += "<span class=\"magentapink\">";
			opened = SPAN;
			i += 4;
		} else if (str.compare(i, 5, "\033[36m") == 0) {
			closeLastTag();
			res += "<span class=\"turquoise\">";
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
		} else if (str.compare(i, 7, "\033[1;35m") == 0) {
			closeLastTag();
			res += "<b class=\"pink\">";
			opened = B;
			i += 6;
		} else if (str.compare(i, 7, "\033[1;36m") == 0) {
			closeLastTag();
			res += "<b class=\"turquoise\">";
			opened = B;
			i += 6;
		} else if (str.compare(i, 3, "\033[m") == 0) {
			closeLastTag();
			opened = NONE;
			i += 2;
		} else
			htmlSpecialChars(res, str[i]);
	}
	closeLastTag();
	return res;
}

void Sim::logs() {
	if (!Session::open() || Session::user_type > UTYPE_ADMIN)
		return error403();

	baseTemplate("Logs", "body{margin-left:20px}");

	// TODO: more logs and show less, but add "show more" button???
	// TODO: active updating logs
	constexpr int BYTES_TO_READ = 16384;
	constexpr int MAX_LINES = 128;

	auto dumpLogTail = [&](const char* filename) {
		FileDescriptor fd {filename, O_RDONLY | O_LARGEFILE};
		if (fd == -1) {
			errlog(__PRETTY_FUNCTION__, ": open()", error(errno));
			return;
		}

		string fdata = getFileContents(fd, -BYTES_TO_READ, -1);

		// The first line is probably not intact so erase it
		if (int(fdata.size()) == BYTES_TO_READ)
			fdata.erase(0, fdata.find('\n'));

		// Cuts fdata to a newline character from the ending
		fdata.erase(std::min(fdata.size(), fdata.rfind('\n')));

		// Shorten to the last MAX_LINES
		auto it = --fdata.end();
		for (uint lines = MAX_LINES; it > fdata.begin(); --it)
			if (*it == '\n' && --lines == 0) {
				fdata.erase(fdata.begin(), ++it);
				break;
			}

		fdata = colorize(fdata);
		append(fdata);
	};

	// Server log
	append("<h2>Server log:</h2>"
		"<pre class=\"logs\">");
	dumpLogTail(SERVER_LOG);
	append("</pre>");

	// Server error log
	append("<h2>Server error log:</h2>"
		"<pre class=\"logs\">");
	dumpLogTail(SERVER_ERROR_LOG);
	append("</pre>");

	// Judge log
	append("<h2>Judge log:</h2>"
		"<pre class=\"logs\">");
	dumpLogTail(JUDGE_LOG);
	append("</pre>");

	// Judge error log
	append("<h2>Judge error log:</h2>"
		"<pre class=\"logs\">");
	dumpLogTail(JUDGE_ERROR_LOG);
	append("</pre>"

	// Script used to scroll down the logs
		"<script>"
			"$(\".logs\").each(function(){"
				"$(this).scrollTop($(this)[0].scrollHeight);"
			"});"
		"</script>");
}
