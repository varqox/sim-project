#include "sim/sessions/session.hh"
#include "sim/random.hh"
#include "simlib/debug.hh"
#include "simlib/time.hh"
#include "src/web_server/old/sim.hh"

#include <chrono>

using sim::sessions::Session;
using sim::users::User;
using std::string;

namespace web_server::old {

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
                              "FROM sessions s, users u "
                              "WHERE s.id=? AND expires>=?"
                              " AND u.id=s.user_id");
    stmt.bind_and_execute(session_id, mysql_date());

    decltype(Session::ip) session_ip;
    decltype(Session::user_agent) user_agent;
    decltype(User::type) s_u_type;
    stmt.res_bind_all(
        session_csrf_token, session_user_id, session_data, s_u_type, session_username,
        session_ip, user_agent);

    if (stmt.next()) {
        session_user_type = s_u_type;
        return (session_is_open = true);
    }

    resp.cookies.set("session", "", 0); // Delete cookie
    return false;
}

void Sim::session_create_and_open(StringView user_id, bool short_session) {
    STACK_UNWINDING_MARK;

    session_close();

    // Remove obsolete sessions
    mysql.prepare("DELETE FROM sessions WHERE expires<?").bind_and_execute(mysql_date());

    // Create a record in database
    auto stmt = mysql.prepare("INSERT IGNORE sessions(id, csrf_token, user_id,"
                              " data, ip, user_agent, expires) "
                              "VALUES(?,?,?,'',?,?,?)");

    session_csrf_token = sim::generate_random_token(decltype(Session::csrf_token)::max_len);
    auto user_agent = request.headers.get("User-Agent");
    auto exp_tp = std::chrono::system_clock::now() +
        (short_session ? Session::short_session_max_lifetime
                       : Session::long_session_max_lifetime);

    do {
        session_id = sim::generate_random_token(decltype(Session::id)::max_len);
        stmt.bind_and_execute(
            session_id, session_csrf_token, user_id, client_ip, user_agent,
            mysql_date(exp_tp));

    } while (stmt.affected_rows() == 0);

    auto exp_time_t = std::chrono::system_clock::to_time_t(exp_tp);
    if (short_session) {
        exp_time_t = -1; // -1 causes cookies.set() to not set Expire= field
    }

    // There is no better method than looking on the referer
    bool is_https = has_prefix(request.headers["referer"], "https://");
    resp.cookies.set(
        "csrf_token", session_csrf_token.to_string(), exp_time_t, "/", "", false, is_https);
    resp.cookies.set("session", session_id.to_string(), exp_time_t, "/", "", true, is_https);
    session_is_open = true;
}

void Sim::session_destroy() {
    STACK_UNWINDING_MARK;

    if (not session_open()) {
        return;
    }

    try {
        mysql.prepare("DELETE FROM sessions WHERE id=?").bind_and_execute(session_id);
    } catch (const std::exception& e) {
        ERRLOG_CATCH(e);
    }

    resp.cookies.set("csrf_token", "", 0); // Delete cookie
    resp.cookies.set("session", "", 0); // Delete cookie
    session_is_open = false;
}

void Sim::session_close() {
    STACK_UNWINDING_MARK;

    if (!session_is_open) {
        return;
    }

    session_is_open = false;
    // TODO: add a marker of a change used to omit unnecessary updates
    auto stmt = mysql.prepare("UPDATE sessions SET data=? WHERE id=?");
    stmt.bind_and_execute(session_data, session_id);
}

} // namespace web_server::old
