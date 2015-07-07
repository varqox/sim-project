#pragma once

#include "http_request.h"
#include "http_response.h"

#include "../include/db.h"
#include "../include/memory.h"

// Every object is independent, thread-safe
class Sim {
private:
	Sim(const Sim&);
	Sim& operator=(const Sim&);

	class Contest;
	class Session;
	class Template;
	class User;

	UniquePtr<DB::Connection> db_conn_;
	std::string client_ip_;
	const server::HttpRequest* req_;
	server::HttpResponse resp_;
	// Modules
	Contest *contest;
	Session *session;
	User *user;

	DB::Connection& db_conn() const { return *db_conn_; }

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
	 * @brief Converts @p type to int
	 *
	 * @param type "admin" or "teacher" or "normal"
	 * @return 0 if type is "admin", 1 if type is "teacher", 2 in other case
	 */
	static int userTypeToRank(const std::string& type);

	/**
	 * @brief Returns user type
	 * @details Uses userTypeToRank()
	 *
	 * @param user_id user id
	 * @return user rank
	 */
	int getUserRank(const std::string& user_id);

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
