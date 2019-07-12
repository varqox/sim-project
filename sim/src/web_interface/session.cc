#include "sim.h"

using std::string;

bool Sim::session_open() {
	STACK_UNWINDING_MARK;

	if (session_is_open)
		return true;

	session_id = request.getCookie("session");
	// Cookie does not exist (or has no value)
	if (session_id.size == 0)
		return false;

	try {
		auto stmt = mysql.prepare("SELECT csrf_token, user_id, data, type,"
		                          " username, ip, user_agent "
		                          "FROM session s, users u "
		                          "WHERE s.id=? AND expires>=?"
		                          " AND u.id=s.user_id");
		stmt.bindAndExecute(session_id, mysql_date());

		InplaceBuff<SESSION_IP_LEN + 1> session_ip;
		InplaceBuff<4096> user_agent;
		EnumVal<UserType> s_u_type;
		stmt.res_bind_all(session_csrf_token, session_user_id, session_data,
		                  s_u_type, session_username, session_ip, user_agent);

		if (stmt.next()) {
			session_user_type = s_u_type;
			// If no session injection
			if (client_ip == session_ip &&
			    request.headers.isEqualTo("User-Agent", user_agent)) {
				return (session_is_open = true);
			}
		}

	} catch (...) {
		ERRLOG_AND_FORWARD();
	}

	resp.setCookie("session", "", 0); // Delete cookie
	return false;
}

void Sim::session_create_and_open(StringView user_id, bool temporary_session) {
	STACK_UNWINDING_MARK;

	session_close();

	// Remove obsolete sessions
	mysql.prepare("DELETE FROM session WHERE expires<?")
	   .bindAndExecute(mysql_date());

	// Create a record in database
	auto stmt = mysql.prepare("INSERT IGNORE session(id, csrf_token, user_id,"
	                          " data, ip, user_agent, expires) "
	                          "VALUES(?,?,?,'',?,?,?)");

	session_csrf_token = generate_random_token(SESSION_CSRF_TOKEN_LEN);
	auto user_agent = request.headers.get("User-Agent");
	time_t exp_time =
	   time(nullptr) +
	   (temporary_session ? TMP_SESSION_MAX_LIFETIME : SESSION_MAX_LIFETIME);
	auto exp_datetime = mysql_date(exp_time);

	stmt.bind(1, session_csrf_token);
	stmt.bind(2, user_id);
	stmt.bind(3, client_ip);
	stmt.bind(4, user_agent);
	stmt.bind(5, exp_datetime);

	do {
		session_id = generate_random_token(SESSION_ID_LEN);
		stmt.bind(0, session_id);
		stmt.fixAndExecute();

	} while (stmt.affected_rows() == 0);

	if (temporary_session)
		exp_time = -1; // -1 causes setCookie() to not set Expire= field

	// There is no better method than looking on the referer
	bool is_https = hasPrefix(request.headers["referer"], "https://");
	resp.setCookie("csrf_token", session_csrf_token.to_string(), exp_time, "/",
	               "", false, is_https);
	resp.setCookie("session", session_id.to_string(), exp_time, "/", "", true,
	               is_https);
	session_is_open = true;
}

void Sim::session_destroy() {
	STACK_UNWINDING_MARK;

	if (not session_open())
		return;

	try {
		mysql.prepare("DELETE FROM session WHERE id=?")
		   .bindAndExecute(session_id);
	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
	}

	resp.setCookie("csrf_token", "", 0); // Delete cookie
	resp.setCookie("session", "", 0); // Delete cookie
	session_is_open = false;
}

void Sim::session_close() {
	STACK_UNWINDING_MARK;

	if (!session_is_open)
		return;

	session_is_open = false;
	try {
		// TODO: add a marker of a change used to omit unnecessary updates
		auto stmt = mysql.prepare("UPDATE session SET data=? WHERE id=?");
		stmt.bindAndExecute(session_data, session_id);

	} catch (...) {
		ERRLOG_AND_FORWARD();
	}
}
