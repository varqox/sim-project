#pragma once

#include "http_request.h"
#include "http_response.h"

#include "../include/db.h"

// Every object is independent, thread-safe
class SIM {
private:
	SIM(const SIM&);
	SIM& operator=(const SIM&);

	class Session;
	class Template;

	DB::Connection *db_conn_;
	std::string client_ip_;
	const server::HttpRequest *req_;
	server::HttpResponse resp_;
	Session *session;

	// sim_errors.cc
	void error403();

	void error404();

	void error500();

	// sim_main.cc
	void mainPage();

	void getStaticFile();

	DB::Connection& db_conn() const { return *db_conn_; }

	void redirect(const std::string& location);

	// sim_user.cc
	void login();

	void logout();

	void signUp();

	// sim_contest.cc
	friend class Contest;

	void contest();

	void submission();

public:
	SIM();

	~SIM();

	server::HttpResponse handle(std::string client_ip,
			const server::HttpRequest& req);
};
