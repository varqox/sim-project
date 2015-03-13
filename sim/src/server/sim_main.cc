#include "sim.h"
#include "sim_template.h"
#include "sim_session.h"

#include "../include/string.h"
#include "../include/debug.h"
#include "../include/time.h"

#include <sys/stat.h>
#include <unistd.h>
#include <cstring>

using std::string;

SIM::SIM() : db_conn_(NULL), client_ip_(), req_(NULL),
		resp_(server::HttpResponse::TEXT), session(new Session(*this)) {
	char *host, *user, *password, *database;

	FILE *conf = fopen("db.config", "r");
	if (conf == NULL) {
		eprintf("Cannot open file: 'db.config'\n");
		return;
	}

	// Get pass
	size_t x1 = 0, x2 = 0, x3 = 0, x4 = 0;
	if (getline(&user, &x1, conf) == -1 || getline(&password, &x2, conf) == -1 ||
			getline(&database, &x3, conf) == -1 ||
			getline(&host, &x4, conf) == -1) {
		eprintf("Failed to get database config\n");
		fclose(conf);
		return;
	}
	fclose(conf);
	user[strlen(user)-1] = password[strlen(password)-1] = '\0';
	database[strlen(database)-1] = host[strlen(host)-1] = '\0';

	// Connect
	try {
		db_conn_ = new DB::Connection(host, user, password, database);
	} catch (...) {
		eprintf("Failed to connect to database\n");
		db_conn_ = NULL;
	}

	// Free resources
	free(host);
	free(user);
	free(password);
	free(database);
}

SIM::~SIM() {
	if (db_conn_)
		delete db_conn_;
	delete session;
}

server::HttpResponse SIM::handle(string client_ip, const server::HttpRequest& req) {
	client_ip_.swap(client_ip);
	req_ = &req;
	resp_ = server::HttpResponse(server::HttpResponse::TEXT);

	E("%s\n", req.target.c_str());

	try {
		if (0 == compareTo(req.target, 1, '/', "kit"))
			getStaticFile();
		else if (0 == compareTo(req.target, 1, '/', "login"))
			login();
		else if (0 == compareTo(req.target, 1, '/', "logout"))
			logout();
		else if (0 == compareTo(req.target, 1, '/', "signup"))
			signUp();
		else if (0 == compareTo(req.target, 1, '/', "c"))
			contest();
		else if (0 == compareTo(req.target, 1, '/', ""))
			mainPage();
		else
			error404();
	} catch (...) {
		E("\e[31mCaught exception: %s:%d\e[0m\n", __FILE__, __LINE__);
		error500();
	}
	// Make sure that session is closed
	session->close();
	return resp_;
}

void SIM::mainPage() {
	Template templ(*this, "Main page");
	templ << "<div style=\"text-align: center\">\n"
				"<img src=\"/kit/img/SIM-logo.png\" width=\"260\" height=\"336\" alt=\"\">\n"
				"<p style=\"font-size: 30px\">Welcome to SIM</p>\n"
				"<hr>\n"
				"<p>SIM is open source platform for carrying out algorithmic contests</p>\n"
			"</div>\n";
}

void SIM::getStaticFile() {
	string file = "public";
	file += abspath(decodeURI(req_->target, 1, req_->target.find('?'))); // Extract path ignoring query
	E("%s\n", file.c_str());

	resp_.content_type = server::HttpResponse::FILE;

	// Get file stat
	struct stat attr;
	if (stat(file.c_str(), &attr) != -1) {
		// Extract time of last modification
		resp_.headers["Last-Modified"] = date("%a, %d %b %Y %H:%M:%S GMT", attr.st_mtime);
		server::HttpHeaders::const_iterator it = req_->headers.find("if-modified-since");
		struct tm client_mtime;

		// If "If-Modified-Since" header is set and its value is not lower than attr.st_mtime
		if (it != req_->headers.end() && NULL != strptime(it->second.c_str(),
				"%a, %d %b %Y %H:%M:%S GMT", &client_mtime) &&
				timegm(&client_mtime) >= attr.st_mtime) {
			resp_.status_code = "304 Not Modified";
			resp_.content_type = server::HttpResponse::TEXT;
			return;
		}
	}
	resp_.content = file;
}

void SIM::redirect(const string& location) {
	resp_.status_code = "302 Moved Temporarily";
	resp_.headers["Location"] = location;
}
