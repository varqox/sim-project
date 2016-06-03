#include "session.h"

#include <sim/constants.h>
#include <simlib/debug.h>
#include <simlib/logger.h>
#include <simlib/random.h>
#include <simlib/time.h>

using std::string;

bool Session::open() {
	if (is_open)
		return true;

	sid = req->getCookie("session");
	// Cookie does not exist (or have no value)
	if (sid.empty())
		return false;

	try {
		DB::Statement stmt = db_conn.prepare(
				"SELECT user_id, data, type, username, ip, user_agent "
				"FROM session s, users u "
				"WHERE s.id=? AND time>=? AND u.id=s.user_id");
		stmt.setString(1, sid);
		stmt.setString(2, date("%Y-%m-%d %H:%M:%S",
			time(nullptr) - SESSION_MAX_LIFETIME));

		DB::Result res = stmt.executeQuery();
		if (res.next()) {
			user_id = res[1];
			data = res[2];
			user_type = res.getUInt(3);
			username = res[4];

			// If no session injection
			if (client_ip == res[5] &&
				req->headers.isEqualTo("User-Agent", res[6]))
			{
				return (is_open = true);
			}
		}

	} catch (const std::exception& e) {
		ERRLOG_FORWARDING(e);
	}

	resp.setCookie("session", "", 0); // Delete cookie
	return false;
}

string Session::generateId(uint length) {
	constexpr char t[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"0123456789";
	constexpr size_t len = sizeof(t) - 1;

	// Generate random id of length SESSION_ID_LENGTH
	string res(length, '0');
	for (char& c : res)
		c = t[getRandom<int>(0, len - 1)];

	return res;
}

void Session::createAndOpen(const string& _user_id) noexcept(false) {
	close();

	// Remove obsolete sessions
	db_conn.executeUpdate(concat("DELETE FROM `session` WHERE time<'",
		date("%Y-%m-%d %H:%M:%S'", time(nullptr) - SESSION_MAX_LIFETIME)));

	DB::Statement stmt = db_conn.prepare("INSERT IGNORE session "
			"(id, user_id, data, ip, user_agent, time) VALUES(?,?,'',?,?,?)");
	stmt.setString(2, _user_id);
	stmt.setString(3, client_ip);
	stmt.setString(4, req->headers.get("User-Agent"));
	stmt.setString(5, date("%Y-%m-%d %H:%M:%S"));

	do {
		sid = generateId(SESSION_ID_LEN);
		stmt.setString(1, sid); // TODO: parameters preserve!
	} while (stmt.executeUpdate() == 0);

	resp.setCookie("session", sid, time(nullptr) + SESSION_MAX_LIFETIME, "/",
		"", true);
	is_open = true;
}

void Session::destroy() {
	if (!is_open)
		return;

	try {
		DB::Statement stmt = db_conn.prepare("DELETE FROM session WHERE id=?");
		stmt.setString(1, sid);
		stmt.executeUpdate();

	} catch (const std::exception& e) {
		ERRLOG_CAUGHT(e);
	}

	resp.setCookie("session", "", 0); // Delete cookie
	is_open = false;
}

void Session::close() {
	if (!is_open)
		return;

	is_open = false;
	try {
		DB::Statement stmt = db_conn.prepare(
			"UPDATE session SET data=? WHERE id=?");
		stmt.setString(1, data);
		stmt.setString(2, sid);
		stmt.executeUpdate();

	} catch (const std::exception& e) {
		ERRLOG_FORWARDING(e);
	}
}
