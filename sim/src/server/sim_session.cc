#include "sim_session.h"

#include "../include/random.h"
#include "../include/debug.h"
#include "../include/memory.h"
#include "../include/time.h"

#include <cppconn/prepared_statement.h>

using std::string;

SIM::Session::State SIM::Session::open() {
	if (state_ == OK)
		return OK;

	state_ = CLOSED;

	id_ = sim_.req_->getCookie("session");
	// Cookie doesn't exist (or have no value)
	if (id_.empty())
		return CLOSED;

	try {
		UniquePtr<sql::PreparedStatement> pstmt = sim_.db_conn_->mysql()
				->prepareStatement(
				"SELECT user_id,data,ip,user_agent FROM `session` WHERE id=? AND time>=?");
		pstmt->setString(1, id_);
		pstmt->setString(2, date("%Y-%m-%d %H:%M:%S", time(NULL) - SESSION_MAX_LIFETIME));
		pstmt->execute();

		UniquePtr<sql::ResultSet> res = pstmt->getResultSet();
		if (res->next()) {
			user_id = res->getString(1);
			data = res->getString(2);

			// If no session injection
			if (sim_.client_ip_ == res->getString(3) &&
					sim_.req_->headers.isEqualTo("User-Agent", res->getString(4)))
				return (state_ = OK);
		}
	} catch (...) {
		E("Catched exception: %s:%d\n", __FILE__, __LINE__);
	}

	sim_.resp_.setCookie("session", "", 0); // Delete cookie
	return CLOSED;
}

static string generate_id() {
	const char t[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	string res(10, '0');
	for (int i = 0; i < 10; ++i)
		res[i] = t[getRandom(0, sizeof(t) - 1)];
	return res;
}

SIM::Session::State SIM::Session::create(const string& _user_id) {
	close();
	try {
		UniquePtr<sql::PreparedStatement> pstmt = sim_.db_conn_->mysql()
				->prepareStatement("INSERT INTO `session` (id,user_id,ip,user_agent,time) VALUES(?,?,?,?,?)");

		// Remove obsolete sessions
		UniquePtr<sql::Statement>(sim_.db_conn_->mysql()->createStatement())
				->executeUpdate(string("DELETE FROM `session` WHERE time<'")
					.append(date("%Y-%m-%d %H:%M:%S'",
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
		sim_.resp_.setCookie("session", id_, time(NULL) + SESSION_MAX_LIFETIME, "", "", true);
		return (state_ = OK);
	} catch (...) {
		E("Catched exception: %s:%d\n", __FILE__, __LINE__);
	}
	return CLOSED;
}

void SIM::Session::destroy() {
	if (state_ == CLOSED)
		return;

	try {
		UniquePtr<sql::PreparedStatement> pstmt = sim_.db_conn_->mysql()
				->prepareStatement("DELETE FROM `session` WHERE id=?");
		pstmt->setString(1, id_);
		pstmt->executeUpdate();

		sim_.resp_.setCookie("session", "", 0); // Delete cookie
	} catch (...) {
		E("Catched exception: %s:%d\n", __FILE__, __LINE__);
	}
	state_ = CLOSED;
}

void SIM::Session::close() {
	if (state_ == CLOSED)
		return;
	try {
		UniquePtr<sql::PreparedStatement> pstmt = sim_.db_conn_->mysql()
				->prepareStatement("UPDATE `session` SET data=?, time=? WHERE id=?");
		pstmt->setString(1, data);
		pstmt->setString(2, date("%Y-%m-%d %H:%M:%S"));
		pstmt->setString(3, id_);
		pstmt->executeUpdate();
	} catch (...) {
		E("Catched exception: %s:%d\n", __FILE__, __LINE__);
	}
	state_ = CLOSED;
}
