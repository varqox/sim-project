#include "session.h"

#include <simlib/debug.h>
#include <simlib/logger.h>
#include <simlib/random.h>
#include <simlib/time.h>

using std::string;

bool Session::open() {
	if (is_open)
		return true;

	sid = req->getCookie("session");
	// Cookie does not exist (or has no value)
	if (sid.empty())
		return false;

	try {
		MySQL::Statement stmt = db_conn.prepare(
				"SELECT csrf_token, user_id, data, type, username, ip, "
					"user_agent "
				"FROM session s, users u "
				"WHERE s.id=? AND time>=? AND u.id=s.user_id");
		stmt.setString(1, sid);
		stmt.setString(2, date("%Y-%m-%d %H:%M:%S",
			time(nullptr) - SESSION_MAX_LIFETIME));

		MySQL::Result res = stmt.executeQuery();
		if (res.next()) {
			csrf_token = res[1];
			user_id = res[2];
			data = res[3];
			user_type = res.getUInt(4);
			username = res[5];

			// If no session injection
			if (client_ip == res[6] &&
				req->headers.isEqualTo("User-Agent", res[7]))
			{
				return (is_open = true);
			}
		}

	} catch (...) {
		ERRLOG_AND_FORWARD();
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

void Session::createAndOpen(const string& _user_id) {
	close();

	// Remove obsolete sessions
	db_conn.executeUpdate(concat("DELETE FROM `session` WHERE time<'",
		date("%Y-%m-%d %H:%M:%S'", time(nullptr) - SESSION_MAX_LIFETIME)));

	csrf_token = generateId(SESSION_CSRF_TOKEN_LEN);

	MySQL::Statement stmt = db_conn.prepare("INSERT IGNORE session "
			"(id, csrf_token, user_id, data, ip, user_agent, time) "
			"VALUES(?,?,?,'',?,?,?)");
	stmt.setString(2, csrf_token);
	stmt.setString(3, _user_id);
	stmt.setString(4, client_ip);
	stmt.setString(5, req->headers.get("User-Agent"));
	stmt.setString(6, date());

	do {
		sid = generateId(SESSION_ID_LEN);
		stmt.setString(1, sid); // TODO: parameters preserve!
	} while (stmt.executeUpdate() == 0);

	resp.setCookie("csrf_token", csrf_token,
		time(nullptr) + SESSION_MAX_LIFETIME, "/");
	resp.setCookie("session", sid, time(nullptr) + SESSION_MAX_LIFETIME, "/",
		"", true);
	is_open = true;
}

void Session::destroy() {
	if (!is_open)
		return;

	try {
		MySQL::Statement stmt
			{db_conn.prepare("DELETE FROM session WHERE id=?")};
		stmt.setString(1, sid);
		stmt.executeUpdate();

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
	}

	resp.setCookie("csrf_token", "", 0); // Delete cookie
	resp.setCookie("session", "", 0); // Delete cookie
	is_open = false;
}

void Session::close() {
	if (!is_open)
		return;

	is_open = false;
	try {
		MySQL::Statement stmt = db_conn.prepare(
			"UPDATE session SET data=? WHERE id=?");
		stmt.setString(1, data);
		stmt.setString(2, sid);
		stmt.executeUpdate();

	} catch (...) {
		ERRLOG_AND_FORWARD();
	}
}
