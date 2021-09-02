#include "sim/sessions/session.hh"
#include "sim/random.hh"
#include "simlib/debug.hh"
#include "simlib/time.hh"
#include "src/web_server/old/sim.hh"

#include <chrono>
#include <optional>

using sim::sessions::Session;
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

    resp.cookies.set("session", "", 0, std::nullopt, false, false); // Delete cookie
    return false;
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
