#include "sim_contest.h"
#include "sim_session.h"
#include "sim_template.h"
#include "sim_user.h"

#include "../simlib/include/debug.h"
#include "../simlib/include/filesystem.h"
#include "../simlib/include/time.h"

#include <cppconn/prepared_statement.h>

using std::string;

Sim::Sim() : db_conn_(DB::createConnectionUsingPassFile(".db.config")),
		client_ip_(), req_(NULL), resp_(server::HttpResponse::TEXT),
		contest(NULL), session(NULL), user(NULL) {
	// Because of exception safety (we do not want to make memory leak)
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

	E("%s\n", req.target.c_str());

	try {
		if (0 == compareTo(req.target, 1, '/', "kit"))
			getStaticFile();

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
		E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__, __LINE__,
			e.what());
		error500();

	} catch (...) {
		E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
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
	E("%s\n", file.c_str());

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
	resp_.content_type = server::HttpResponse::FILE;
}

void Sim::redirect(const string& location) {
	resp_.status_code = "302 Moved Temporarily";
	resp_.headers["Location"] = location;
}

int Sim::getUserType(const string& user_id) {
	try {
		UniquePtr<sql::PreparedStatement> pstmt(db_conn()->
			prepareStatement("SELECT type FROM users WHERE id=?"));
		pstmt->setString(1, user_id);

		UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
		if (res->next())
			return res->getUInt(1);

	} catch (const std::exception& e) {
		E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__, __LINE__,
			e.what());

	} catch (...) {
		E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
	}

	return 3;
}
