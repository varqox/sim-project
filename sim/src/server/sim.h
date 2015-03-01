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
	const server::HttpRequest *req_;
	server::HttpResponse resp_;

	void error403();
	void error404();

	void mainPage();
	void getStaticFile();

	void login();
	void logout();
	void singUp();

	void contest();
	void problems();
	void submit();
	void submission();
	void submissions();
	void rank();
public:
	SIM();
	server::HttpResponse handle(const server::HttpRequest& req);
	~SIM();
};
