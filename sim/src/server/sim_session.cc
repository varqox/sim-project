#include "sim_session.h"

#include <cppconn/prepared_statement.h>
#include <simlib/logger.h>
#include <simlib/random.h>
#include <simlib/time.h>

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
		DB::Statement stmt = sim_.db_conn.prepare(
				"SELECT user_id, data, type, username, ip, user_agent "
				"FROM session s, users u "
				"WHERE s.id=? AND time>=? AND u.id=s.user_id");
		stmt.bind(1, id_);
		stmt.bind(2, date("%Y-%m-%d %H:%M:%S",
			time(nullptr) - SESSION_MAX_LIFETIME));

		DB::Result res = stmt.executeQuery();
		if (res.next()) {
			user_id = res[1];
			data = res[2];
			user_type = res.getUInt(3);
			username = res[4];

			// If no session injection
			if (sim_.client_ip_ == res[5] &&
				sim_.req_->headers.isEqualTo("User-Agent", res[6]))
			{
				return (state_ = OK);
			}
		}

	} catch (const std::exception& e) {
		errlog("Caught exception: ", __FILE__, ':', toString(__LINE__), " -> ",
			e.what());
	}

	sim_.resp_.setCookie("session", "", 0); // Delete cookie
	return FAIL;
}

string Sim::Session::generateId() {
	constexpr char t[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"0123456789";
	constexpr size_t len = sizeof(t) - 1;

	// Generate random id of length SESSION_ID_LENGTH
	string res(SESSION_ID_LENGTH, '0');
	for (char& c : res)
		c = t[getRandom<int>(0, len - 1)];

	return res;
}

Sim::Session::State Sim::Session::create(const string& _user_id) {
	close();
	state_ = FAIL;
	try {
		DB::Statement stmt = sim_.db_conn.prepare("INSERT session "
				"(id, user_id, ip, user_agent,time) VALUES(?,?,?,?,?)");

		// Remove obsolete sessions
		sim_.db_conn.executeUpdate(concat("DELETE FROM `session` WHERE time<'",
			date("%Y-%m-%d %H:%M:%S'", time(nullptr) - SESSION_MAX_LIFETIME)));
		stmt.bind(2, _user_id);
		stmt.bind(3, sim_.client_ip_);
		stmt.bind(4, sim_.req_->headers.get("User-Agent"));
		stmt.bind(5, date("%Y-%m-%d %H:%M:%S"));

		for (;;) {
			id_ = generateId();
			stmt.bind(1, id_); // TODO: parameters preserve!

			try {
				stmt.executeUpdate(); // TODO: INSERT IGNORE
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
		DB::Statement stmt = sim_.db_conn.prepare(
			"DELETE FROM session WHERE id=?");
		stmt.bind(1, id_);
		stmt.executeUpdate();

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
		DB::Statement stmt = sim_.db_conn.prepare(
			"UPDATE session SET data=? WHERE id=?");
		stmt.bind(1, data);
		stmt.bind(2, id_);
		stmt.executeUpdate();

	} catch (const std::exception& e) {
		errlog("Caught exception: ", __FILE__, ':', toString(__LINE__), " -> ",
			e.what());
	}
}
