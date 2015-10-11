#pragma once

#include "http_request.h"
#include "http_response.h"

#include "../include/db.h"

#include <utime.h>

// Every object is independent, thread-safe
class Sim {
private:
	Sim(const Sim&);
	Sim& operator=(const Sim&);

	class Contest;
	class Session;
	class Template;
	class User;

	DB::Connection db_conn;
	std::string client_ip_;
	const server::HttpRequest* req_;
	server::HttpResponse resp_;
	// Modules
	Contest *contest;
	Session *session;
	User *user;

	// sim_errors.cc
	void error403();

	void error404();

	void error500();

	// sim_main.cc
	void mainPage();

	void getStaticFile();

	/**
	 * @brief Sets headers to make a redirection
	 * @details Does not clear response headers and contents
	 *
	 * @param location URL address where to redirect
	 */
	void redirect(const std::string& location);

	/**
	 * @brief Returns user type
	 *
	 * @param user_id user id
	 * @return user type
	 */
	int getUserType(const std::string& user_id);

	// Notifies judge server that there are submissions to judge
	static void notifyJudgeServer() { utime("judge-machine.notify", nullptr); }

public:
	Sim();

	~Sim();

	/**
	 * @brief Handles request
	 * @details Takes requests, handle it and returns response
	 *
	 * @param client_ip IP address of the client
	 * @param req request
	 *
	 * @return response
	 */
	server::HttpResponse handle(std::string client_ip,
			const server::HttpRequest& req);
};
