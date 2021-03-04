#include "context.hh"
#include "simlib/string_view.hh"
#include "simlib/time.hh"
#include "src/web_server/http/response.hh"

#include <optional>

using web_server::http::Response;

namespace web_server::web_worker {

void Context::open_session() {
    session = std::nullopt;
    auto session_id = request.get_cookie(Session::id_cookie_name);
    if (session_id.empty()) {
        return; // Optimization (no mysql query) for empty or nonexistent cookie
    }
    Session s;
    auto stmt =
        mysql.prepare("SELECT s.csrf_token, s.user_id, u.type, u.username, s.data FROM "
                      "sessions s JOIN users u ON u.id=s.user_id WHERE s.id=? AND expires>=?");
    stmt.bind_and_execute(session_id, mysql_date());
    stmt.res_bind_all(s.csrf_token, s.user_id, s.user_type, s.username, s.data);
    if (not stmt.next()) {
        // Session expired or was deleted
        cookie_changes.set(Session::id_cookie_name, "", 0);
        return;
    }
    s.id = session_id;
    s.orig_data = s.data;
    session = s;
}

void Context::close_session() {
    assert(session.has_value());
    if (session->data != session->orig_data) {
        auto stmt = mysql.prepare("UPDATE sessions SET data=? WHERE id=?");
        stmt.bind_and_execute(session->data, session->id);
    }
    session = std::nullopt;
}

Response Context::response(StringView status_code, StringView content) {
    auto resp = Response{Response::TEXT, status_code};
    resp.content = content;
    resp.cookies = std::move(cookie_changes);
    return resp;
}

} // namespace web_server::web_worker
