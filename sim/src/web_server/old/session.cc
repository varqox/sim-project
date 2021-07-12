#include "sim/sessions/session.hh"
#include "sim/random.hh"
#include "simlib/debug.hh"
#include "simlib/time.hh"
#include "src/web_server/old/sim.hh"

#include <chrono>
#include <optional>

using sim::sessions::Session;
using sim::users::User;
using std::string;

namespace web_server::old {

bool Sim::session_open() {
    STACK_UNWINDING_MARK;

    if (session.has_value()) {
        return true;
    }

    decltype(session)::value_type ses;
    ses.id = request.get_cookie("session");
    // Cookie does not exist (or has no value)
    if (ses.id.size == 0) {
        return false;
    }

    auto stmt = mysql.prepare("SELECT csrf_token, user_id, data, type, username "
                              "FROM sessions s, users u "
                              "WHERE s.id=? AND expires>=? AND u.id=s.user_id");
    stmt.bind_and_execute(ses.id, mysql_date());

    stmt.res_bind_all(ses.csrf_token, ses.user_id, ses.data, ses.user_type, ses.username);

    if (stmt.next()) {
        ses.orig_data = ses.data;
        session = ses;
        return true;
    }

    resp.cookies.set("session", "", 0); // Delete cookie
    return false;
}

void Sim::session_create_and_open(decltype(session->user_id) user_id, bool short_session) {
    STACK_UNWINDING_MARK;

    session_close();

    // Remove obsolete sessions
    mysql.prepare("DELETE FROM sessions WHERE expires<?").bind_and_execute(mysql_date());

    // Create a record in database
    auto stmt = mysql.prepare("INSERT IGNORE sessions(id, csrf_token, user_id,"
                              " data, ip, user_agent, expires) "
                              "VALUES(?,?,?,'','',?,?)");

    decltype(session)::value_type ses;
    ses.user_id = user_id;
    ses.csrf_token = sim::generate_random_token(decltype(Session::csrf_token)::max_len);
    auto user_agent = request.headers.get("User-Agent");
    auto exp_tp = std::chrono::system_clock::now() +
        (short_session ? Session::short_session_max_lifetime
                       : Session::long_session_max_lifetime);

    do {
        ses.id = sim::generate_random_token(decltype(Session::id)::max_len);
        stmt.bind_and_execute(
            ses.id, ses.csrf_token, ses.user_id, user_agent, mysql_date(exp_tp));

    } while (stmt.affected_rows() == 0);

    auto exp_time_t = std::chrono::system_clock::to_time_t(exp_tp);
    if (short_session) {
        exp_time_t = -1; // -1 causes cookies.set() to not set Expire= field
    }

    // There is no better method than looking on the referer
    bool is_https = has_prefix(request.headers["referer"], "https://");
    resp.cookies.set(
        "csrf_token", ses.csrf_token.to_string(), exp_time_t, "/", "", false, is_https);
    resp.cookies.set("session", ses.id.to_string(), exp_time_t, "/", "", true, is_https);
    session = ses;
}

void Sim::session_destroy() {
    STACK_UNWINDING_MARK;

    if (not session_open()) {
        return;
    }

    try {
        mysql.prepare("DELETE FROM sessions WHERE id=?").bind_and_execute(session->id);
    } catch (const std::exception& e) {
        ERRLOG_CATCH(e);
    }

    resp.cookies.set("csrf_token", "", 0); // Delete cookie
    resp.cookies.set("session", "", 0); // Delete cookie
    session = std::nullopt;
}

void Sim::session_close() {
    STACK_UNWINDING_MARK;

    if (!session.has_value()) {
        return;
    }
    if (session->data != session->orig_data) {
        auto stmt = mysql.prepare("UPDATE sessions SET data=? WHERE id=?");
        stmt.bind_and_execute(session->data, session->id);
    }
    session = std::nullopt;
}

} // namespace web_server::old
