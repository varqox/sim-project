#include "sim/random.hh"
#include "sim/session.hh"
#include "simlib/debug.hh"
#include "src/web_interface/sim.hh"

using sim::User;
using std::string;

bool Sim::session_open() {
    STACK_UNWINDING_MARK;

    if (session_is_open) {
        return true;
    }

    session_id = request.get_cookie("session");
    // Cookie does not exist (or has no value)
    if (session_id.size == 0) {
        return false;
    }

    auto stmt = mysql.prepare("SELECT csrf_token, user_id, data, type,"
                              " username, ip, user_agent "
                              "FROM session s, users u "
                              "WHERE s.id=? AND expires>=?"
                              " AND u.id=s.user_id");
    stmt.bind_and_execute(session_id, mysql_date());

    decltype(sim::Session::ip) session_ip;
    decltype(sim::Session::user_agent) user_agent;
    decltype(User::type) s_u_type;
    stmt.res_bind_all(
        session_csrf_token, session_user_id, session_data, s_u_type, session_username,
        session_ip, user_agent);

    if (stmt.next()) {
        session_user_type = s_u_type;
        return (session_is_open = true);
    }

    resp.set_cookie("session", "", 0); // Delete cookie
    return false;
}

void Sim::session_create_and_open(StringView user_id, bool temporary_session) {
    STACK_UNWINDING_MARK;

    session_close();

    // Remove obsolete sessions
    mysql.prepare("DELETE FROM session WHERE expires<?").bind_and_execute(mysql_date());

    // Create a record in database
    auto stmt = mysql.prepare("INSERT IGNORE session(id, csrf_token, user_id,"
                              " data, ip, user_agent, expires) "
                              "VALUES(?,?,?,'',?,?,?)");

    session_csrf_token = generate_random_token(decltype(sim::Session::csrf_token)::max_len);
    auto user_agent = request.headers.get("User-Agent");
    time_t exp_time =
        time(nullptr) + (temporary_session ? TMP_SESSION_MAX_LIFETIME : SESSION_MAX_LIFETIME);
    auto exp_datetime = mysql_date(exp_time);

    do {
        session_id = generate_random_token(decltype(sim::Session::id)::max_len);
        stmt.bind_and_execute(
            session_id, session_csrf_token, user_id, client_ip, user_agent, exp_datetime);

    } while (stmt.affected_rows() == 0);

    if (temporary_session) {
        exp_time = -1; // -1 causes set_cookie() to not set Expire= field
    }

    // There is no better method than looking on the referer
    bool is_https = has_prefix(request.headers["referer"], "https://");
    resp.set_cookie(
        "csrf_token", session_csrf_token.to_string(), exp_time, "/", "", false, is_https);
    resp.set_cookie("session", session_id.to_string(), exp_time, "/", "", true, is_https);
    session_is_open = true;
}

void Sim::session_destroy() {
    STACK_UNWINDING_MARK;

    if (not session_open()) {
        return;
    }

    try {
        mysql.prepare("DELETE FROM session WHERE id=?").bind_and_execute(session_id);
    } catch (const std::exception& e) {
        ERRLOG_CATCH(e);
    }

    resp.set_cookie("csrf_token", "", 0); // Delete cookie
    resp.set_cookie("session", "", 0); // Delete cookie
    session_is_open = false;
}

void Sim::session_close() {
    STACK_UNWINDING_MARK;

    if (!session_is_open) {
        return;
    }

    session_is_open = false;
    // TODO: add a marker of a change used to omit unnecessary updates
    auto stmt = mysql.prepare("UPDATE session SET data=? WHERE id=?");
    stmt.bind_and_execute(session_data, session_id);
}
