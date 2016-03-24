#pragma once

#include "contest.h"
#include "user.h"

// Every object is independent, objects can be used in multi-thread program
// as long as one is not used by two threads simultaneously
class Sim final : private Contest,  private User {
public:
	Sim() = default;

	Sim(const Sim&) = delete;
	Sim(Sim&&) = delete;
	Sim& operator=(const Sim&) = delete;
	Sim& operator=(Sim&&) = delete;

	~Sim() = default;

	/**
	 * @brief Handles request
	 * @details Takes requests, handle it and returns response.
	 *   This function is not thread-safe
	 *
	 * @param client_ip IP address of the client
	 * @param req request
	 *
	 * @return response
	 */
	server::HttpResponse handle(std::string _client_ip,
		const server::HttpRequest& request);

private:
	// Pages
	// sim_main.cc
	void mainPage();

	void getStaticFile();

	void logs();

	// See documentation in sim_base.h
	void redirect(std::string location) {
		resp.status_code = "302 Moved Temporarily";
		resp.headers["Location"] = std::move(location);
	}
};
