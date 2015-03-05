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

	// sim_user.cc
	void login();

	void logout();

	void signUp();

	// sim_contest.cc
	void contest();

	void problems();

	void submit();

	void submission();

	void submissions();

	void rank();

public:
	SIM();

	~SIM();

	server::HttpResponse handle(std::string client_ip,
			const server::HttpRequest& req);
};
