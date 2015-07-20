#include "sim_session.h"

#include "../include/debug.h"
#include "../include/random.h"
#include "../include/time.h"

#include <cppconn/prepared_statement.h>
#include <cstring>

using std::string;

Sim::Session::State Sim::Session::open() {
	if (state_ != CLOSED)
		return state_;

	state_ = FAIL;

	id_ = sim_.req_->getCookie("session");
	// Cookie does not exist (or have no value)
	if (id_.empty())
		return FAIL;

	try {
		UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
			prepareStatement(
				"SELECT user_id, data, type, username, ip, user_agent "
				"FROM session s, users u WHERE s.id=? AND time>=? AND u.id=s.user_id"));
		pstmt->setString(1, id_);
		pstmt->setString(2, date("%Y-%m-%d %H:%M:%S",
				time(NULL) - SESSION_MAX_LIFETIME));

		UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
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
		E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__, __LINE__,
			e.what());

	} catch (...) {
		E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
	}

	sim_.resp_.setCookie("session", "", 0); // Delete cookie
	return FAIL;
}

static string generate_id() {
	const char t[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	const size_t len = strlen(t), SESSION_ID_LENGTH = 10;

	string res(SESSION_ID_LENGTH, '0');

	for (size_t i = 0; i < SESSION_ID_LENGTH; ++i)
		res[i] = t[getRandom(0, len - 1)];

	return res;
}

Sim::Session::State Sim::Session::create(const string& _user_id) {
	close();
	try {
		UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
			prepareStatement("INSERT INTO session "
				"(id, user_id, ip, user_agent,time) VALUES(?,?,?,?,?)"));

		// Remove obsolete sessions
		UniquePtr<sql::Statement>(sim_.db_conn()->createStatement())->
			executeUpdate(string("DELETE FROM `session` WHERE time<'").
				append(date("%Y-%m-%d %H:%M:%S'",
					time(NULL) - SESSION_MAX_LIFETIME)));
		pstmt->setString(2, _user_id);
		pstmt->setString(3, sim_.client_ip_);
		pstmt->setString(4, sim_.req_->headers.get("User-Agent"));
		pstmt->setString(5, date("%Y-%m-%d %H:%M:%S"));

		for (;;) {
			id_ = generate_id();
			pstmt->setString(1, id_);

			try {
				pstmt->executeUpdate();
				break;

			} catch (...) {
				continue;
			}
		}

		sim_.resp_.setCookie("session", id_, time(NULL) + SESSION_MAX_LIFETIME, "/", "", true);
		state_ = OK;

	} catch (const std::exception& e) {
		E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__, __LINE__,
			e.what());
		state_ = FAIL;

	} catch (...) {
		E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
		state_ = FAIL;
	}

	return state_;
}

void Sim::Session::destroy() {
	if (state_ != OK)
		return;

	try {
		UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
			prepareStatement("DELETE FROM session WHERE id=?"));
		pstmt->setString(1, id_);
		pstmt->executeUpdate();

		sim_.resp_.setCookie("session", "", 0); // Delete cookie

	} catch (const std::exception& e) {
		E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__, __LINE__,
			e.what());

	} catch (...) {
		E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
	}

	state_ = CLOSED;
}

void Sim::Session::close() {
	if (state_ == OK) {
		try {
			UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
				prepareStatement("UPDATE session SET data=?, time=? WHERE id=?"));
			pstmt->setString(1, data);
			pstmt->setString(2, date("%Y-%m-%d %H:%M:%S"));
			pstmt->setString(3, id_);
			pstmt->executeUpdate();

		} catch (const std::exception& e) {
			E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__, __LINE__,
				e.what());

		} catch (...) {
			E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
		}
	}

	state_ = CLOSED;
}
