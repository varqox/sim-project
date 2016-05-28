#pragma once

#include "http_request.h"
#include "http_response.h"

#include <sim/cpp_syntax_highlighter.h>
#include <sim/db.h>
#include <simlib/parsers.h>
#include <utime.h>

class SimBase {
protected:
	DB::Connection db_conn;
	std::string client_ip; // TODO: put in request?
	const server::HttpRequest* req = nullptr;
	server::HttpResponse resp;
	RequestURIParser url_args {""};
	CppSyntaxHighlighter cpp_syntax_highlighter;

	SimBase() : db_conn(DB::createConnectionUsingPassFile(".db.config")) {}

	SimBase(const SimBase&) = delete;
	SimBase(SimBase&&) = delete;
	SimBase& operator=(const SimBase&) = delete;
	SimBase& operator=(SimBase&& s) = delete;

	virtual ~SimBase() = default;

	/**
	 * @brief Sets headers to make a redirection
	 * @details Does not clear response headers and contents
	 *
	 * @param location URL address where to redirect
	 */
	virtual void redirect(std::string location) = 0;

	// Notifies judge server that there are submissions to judge
	static void notifyJudgeServer() noexcept {
		utime("judge-machine.notify", nullptr);
	}

	static std::string submissionStatusDescription(const std::string& status);
};
