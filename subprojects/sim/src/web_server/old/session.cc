#include "sim.hh"

#include <optional>
#include <sim/mysql/mysql.hh>
#include <sim/random.hh>
#include <sim/sessions/old_session.hh>
#include <sim/sql/sql.hh>
#include <simlib/time.hh>

using std::string;

namespace web_server::old {

bool Sim::session_open() {
    STACK_UNWINDING_MARK;

    if (session.has_value()) {
        return true;
    }

    decltype(session)::value_type ses;
    ses.id = request.get_cookie("session").to_string();
    // Cookie does not exist (or has no value)
    if (ses.id.empty()) {
        return false;
    }
    auto stmt =
        mysql.execute(sim::sql::Select("csrf_token, user_id, data, type, username")
                          .from("sessions s, users u")
                          .where("s.id=? AND expires>=? AND u.id=s.user_id", ses.id, mysql_date()));

    stmt.res_bind(ses.csrf_token, ses.user_id, ses.data, ses.user_type, ses.username);

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
        auto old_mysql = old_mysql::ConnectionView{mysql};
        auto stmt = old_mysql.prepare("UPDATE sessions SET data=? WHERE id=?");
        stmt.bind_and_execute(session->data, session->id);
    }
    session = std::nullopt;
}

} // namespace web_server::old
