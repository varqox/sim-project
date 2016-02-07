#include "sim_session.h"

#include <cppconn/prepared_statement.h>
#include <simlib/logger.h>
#include <simlib/random.h>
#include <simlib/time.h>

using std::string;
using std::unique_ptr;

Sim::Session::State Sim::Session::open() {
	if (state_ != CLOSED)
		return state_;

	state_ = FAIL;

	id_ = sim_.req_->getCookie("session");
	// Cookie does not exist (or have no value)
	if (id_.empty())
		return FAIL;

	try {
		unique_ptr<sql::PreparedStatement> pstmt(sim_.db_conn->
			prepareStatement(
				"SELECT user_id, data, type, username, ip, user_agent "
				"FROM session s, users u "
				"WHERE s.id=? AND time>=? AND u.id=s.user_id"));
		pstmt->setString(1, id_);
		pstmt->setString(2, date("%Y-%m-%d %H:%M:%S",
			time(nullptr) - SESSION_MAX_LIFETIME));

		unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		if (res->next()) {
			user_id = res->getString(1);
			data = res->getString(2);
			user_type = res->getUInt(3);
			username = res->getString(4);

			// If no session injection
			if (sim_.client_ip_ == res->getString(5) &&
					sim_.req_->headers.isEqualTo("User-Agent",
						res->getString(6)))
				return (state_ = OK);
		}

	} catch (const std::exception& e) {
		errlog("Caught exception: ", __FILE__, ':', toString(__LINE__), " -> ",
			e.what());
	}

	sim_.resp_.setCookie("session", "", 0); // Delete cookie
	return FAIL;
}

string Sim::Session::generateId() {
	const char t[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"0123456789";
	constexpr size_t len = sizeof(t) - 1;

	// Generate random id of length SESSION_ID_LENGTH
	string res(SESSION_ID_LENGTH, '0');
	for (auto& c : res)
		c = t[getRandom<int>(0, len - 1)];

	return res;
}

Sim::Session::State Sim::Session::create(const string& _user_id) {
	close();
	state_ = FAIL;
	try {
		unique_ptr<sql::PreparedStatement> pstmt(sim_.db_conn->
			prepareStatement("INSERT INTO session "
				"(id, user_id, ip, user_agent,time) VALUES(?,?,?,?,?)"));

		// Remove obsolete sessions
		unique_ptr<sql::Statement>(sim_.db_conn->createStatement())->
			executeUpdate(concat("DELETE FROM `session` WHERE time<'",
				date("%Y-%m-%d %H:%M:%S'",
					time(nullptr) - SESSION_MAX_LIFETIME)));
		pstmt->setString(2, _user_id);
		pstmt->setString(3, sim_.client_ip_);
		pstmt->setString(4, sim_.req_->headers.get("User-Agent"));
		pstmt->setString(5, date("%Y-%m-%d %H:%M:%S"));

		for (;;) {
			id_ = generateId();
			pstmt->setString(1, id_);

			try {
				pstmt->executeUpdate(); // TODO: INSERT IGNORE
				break;

			} catch (...) {
				continue;
			}
		}

		sim_.resp_.setCookie("session", id_, time(nullptr) +
			SESSION_MAX_LIFETIME, "/", "", true);
		state_ = OK;

	} catch (const std::exception& e) {
		errlog("Caught exception: ", __FILE__, ':', toString(__LINE__), " -> ",
			e.what());
	}

	return state_;
}

void Sim::Session::destroy() {
	if (state_ != OK)
		return;

	try {
		unique_ptr<sql::PreparedStatement> pstmt(sim_.db_conn->
			prepareStatement("DELETE FROM session WHERE id=?"));
		pstmt->setString(1, id_);
		pstmt->executeUpdate();

	} catch (const std::exception& e) {
		errlog("Caught exception: ", __FILE__, ':', toString(__LINE__), " -> ",
			e.what());
	}

	sim_.resp_.setCookie("session", "", 0); // Delete cookie
	state_ = CLOSED;
}

void Sim::Session::close() {
	State old_state = state_;
	state_ = CLOSED;
	if (old_state != OK)
		return;

	try {
		unique_ptr<sql::PreparedStatement> pstmt(sim_.db_conn->
			prepareStatement("UPDATE session SET data=? WHERE id=?"));
		pstmt->setString(1, data);
		pstmt->setString(2, id_);
		pstmt->executeUpdate();

	} catch (const std::exception& e) {
		errlog("Caught exception: ", __FILE__, ':', toString(__LINE__), " -> ",
			e.what());
	}
}
