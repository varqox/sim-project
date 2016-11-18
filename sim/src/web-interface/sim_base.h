#pragma once

#include "http_request.h"
#include "http_response.h"

#include <sim/constants.h>
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
	SimBase& operator=(SimBase&&) = delete;

	virtual ~SimBase() = default;

	/**
	 * @brief Sets headers to make a redirection
	 * @details Does not clear response headers and contents
	 *
	 * @param location URL address where to redirect
	 */
	virtual void redirect(std::string location) = 0;

	/// Sets resp.status_code to @p status_code and resp.content to
	///  @p response_body
	virtual void response(std::string status_code,
		std::string response_body = {}) = 0;


	// Notifies judge server that there are submissions to judge
	static void notifyJudgeServer() noexcept {
		utime("judge-machine.notify", nullptr);
	}

	static std::string submissionStatusAsTd(SubmissionStatus status,
		bool show_final);

	static std::string submissionStatusCSSClass(SubmissionStatus status,
		bool show_final);
};
