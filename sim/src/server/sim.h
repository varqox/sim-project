#pragma once

#include "http_request.h"
#include "http_response.h"

#include <sim/db.h>
#include <utime.h>

// Every object is independent, thread-safe
class Sim {
private:
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

	void logs();

	/**
	 * @brief Sets headers to make a redirection
	 * @details Does not clear response headers and contents
	 *
	 * @param location URL address where to redirect
	 */
	void redirect(const std::string& location);

	// Notifies judge server that there are submissions to judge
	static void notifyJudgeServer() { utime("judge-machine.notify", nullptr); }

public:
	Sim();

	Sim(const Sim&) = delete;

	Sim(Sim&& sim) : db_conn(std::move(sim.db_conn)),
			client_ip_(std::move(sim.client_ip_)), req_(sim.req_),
			resp_(std::move(sim.resp_)), contest(sim.contest),
			session(sim.session), user(sim.user) {
		sim.contest = nullptr;
		sim.session = nullptr;
		sim.user = nullptr;
	}

	Sim& operator=(const Sim&) = delete;

	Sim& operator=(Sim&& sim) {
		db_conn = std::move(sim.db_conn);
		client_ip_ = std::move(sim.client_ip_);
		req_ = sim.req_;
		resp_ = std::move(sim.resp_);
		contest = sim.contest;
		session = sim.session;
		user = sim.user;

		sim.contest = nullptr;
		sim.session = nullptr;
		sim.user = nullptr;
		return *this;
	}

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
