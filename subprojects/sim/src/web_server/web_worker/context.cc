#include "../http/response.hh"
#include "../ui_template.hh"
#include "context.hh"

#include <chrono>
#include <exception>
#include <optional>
#include <sim/mysql/mysql.hh>
#include <sim/random.hh>
#include <sim/sessions/session.hh>
#include <sim/sql/sql.hh>
#include <simlib/file_path.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_view.hh>
#include <simlib/time.hh>

using sim::sql::DeleteFrom;
using sim::sql::InsertIgnoreInto;
using sim::sql::Select;
using sim::sql::Update;
using web_server::http::Response;

namespace {

Response response(
    StringView status_code,
    Response::ContentType response_type,
    decltype(web_server::web_worker::Context::cookie_changes)&& cookie_changes,
    StringView content,
    StringView content_type
) {
    auto resp = Response{response_type, status_code};
    resp.headers["content-type"] = content_type.to_string();
    resp.cookies = std::move(cookie_changes);
    resp.content = content;
    return resp;
}

} // namespace

namespace web_server::web_worker {

void Context::open_session() {
    STACK_UNWINDING_MARK;

    throw_assert(not session);
    auto session_id = request.get_cookie(Session::id_cookie_name);
    if (session_id.empty()) {
        return; // Optimization (no mysql query) for empty or nonexistent cookie
    }
    Session s;
    auto stmt =
        mysql.execute(Select("s.csrf_token, s.user_id, u.type, u.username, s.data")
                          .from("sessions s")
                          .inner_join("users u")
                          .on("u.id=s.user_id")
                          .where("s.id=? AND expires>=?", session_id, utc_mysql_datetime()));
    stmt.res_bind(s.csrf_token, s.user_id, s.user_type, s.username, s.data);
    if (not stmt.next()) {
        // Session expired or was deleted
        cookie_changes.set(Session::id_cookie_name, "", 0, std::nullopt, false, false);
        return;
    }
    s.id = session_id.to_string();
    s.orig_data = s.data;
    session = s;
}

void Context::close_session() {
    STACK_UNWINDING_MARK;
    throw_assert(session);
    if (session->data != session->orig_data) {
        mysql.execute(Update("sessions").set("data=?", session->data).where("id=?", session->id));
    }
    session = std::nullopt;
}

void Context::create_session(
    decltype(Session::user_id) user_id,
    decltype(Session::user_type) user_type,
    decltype(Session::username) username,
    decltype(Session::data) data,
    bool long_exiration
) {
    STACK_UNWINDING_MARK;
    throw_assert(not session);
    // Remove expired sessions
    mysql.execute(DeleteFrom("sessions").where("expires<=?", utc_mysql_datetime()));
    // Create a new session
    Session s = {
        .id = {},
        .csrf_token = {},
        .user_id = user_id,
        .user_type = user_type,
        .username = std::move(username),
        .data = std::move(data),
        .orig_data = {},
    };
    s.orig_data = s.data;
    s.csrf_token = sim::generate_random_token(decltype(s.csrf_token)::len);
    auto session_lifetime =
        (long_exiration ? sim::sessions::Session::LONG_SESSION_MAX_LIFETIME
                        : sim::sessions::Session::SHORT_SESSION_MAX_LIFETIME);
    auto expires_str =
        utc_mysql_datetime_with_offset(std::chrono::seconds{session_lifetime}.count());
    for (;;) {
        s.id = sim::generate_random_token(decltype(s.id)::len);
        auto stmt = mysql.execute(
            InsertIgnoreInto("sessions (id, csrf_token, user_id, data, user_agent, expires)")
                .values(
                    "?, ?, ?, ?, ?, ?",
                    s.id,
                    s.csrf_token,
                    s.user_id,
                    s.data,
                    request.headers.get("user-agent").value_or(""),
                    expires_str
                )
        );
        if (stmt.affected_rows() > 0) {
            break;
        }
    }

    auto exp_time_t = long_exiration ? std::optional{std::chrono::system_clock::to_time_t(
                                           std::chrono::system_clock::now() + session_lifetime
                                       )}
                                     : std::nullopt;

    cookie_changes.set("session", s.id, exp_time_t, "/", true, true);
    cookie_changes.set("csrf_token", s.csrf_token, exp_time_t, "/", false, true);

    session = s;
}

void Context::destroy_session() {
    STACK_UNWINDING_MARK;
    throw_assert(session);
    mysql.execute(DeleteFrom("sessions").where("id=?", session->id));
    // Delete client cookies
    cookie_changes.set("session", "", 0, "/", true, true);
    cookie_changes.set("csrf_token", "", 0, "/", false, true);

    session = std::nullopt;
}

bool Context::session_has_expired() noexcept {
    return !request.get_cookie(Session::id_cookie_name).empty() and !session.has_value();
}

Response Context::response_ok(StringView content, StringView content_type) {
    return response("200 OK", Response::TEXT, std::move(cookie_changes), content, content_type);
}

Response Context::response_file(FilePath path, StringView content_type) {
    return response(
        "200 OK", Response::FILE, std::move(cookie_changes), path.to_cstr(), content_type
    );
}

Response Context::response_json(StringView content) {
    return response(
        "200 OK",
        Response::TEXT,
        std::move(cookie_changes),
        content,
        "application/json; charset=utf-8"
    );
}

Response Context::response_400(StringView content, StringView content_type) {
    return response(
        "400 Bad Request", Response::TEXT, std::move(cookie_changes), content, content_type
    );
}

Response Context::response_403(StringView content, StringView content_type) {
    // In order not to reveal information (e.g. existence of something) only admins may see
    // error 403 and the @p content
    if (session) {
        using UT = sim::users::User::Type;
        // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
        switch (session->user_type) {
        case UT::ADMIN: {
            assert(not session_has_expired());
            return response(
                "403 Forbidden", Response::TEXT, std::move(cookie_changes), content, content_type
            );
        }
        case UT::TEACHER:
        case UT::NORMAL: break;
        }
        std::terminate();
    }
    return response_404();
}

http::Response Context::response_404() {
    static constexpr CStringView session_expired_msg =
        "Your session has expired, please try to sign in and then try again.";
    auto content = session_has_expired() ? session_expired_msg : "";
    return response(
        "404 Not Found",
        Response::TEXT,
        std::move(cookie_changes),
        content,
        "text/plain; charset=utf-8"
    );
}

Response Context::response_ui(StringView title, StringView javascript_code) {
    auto resp = response_ok("", "text/html; charset=utf-8");
    // TODO: merge *_ui_template into one function after getting rid of the old code requiring
    // the split form
    begin_ui_template(
        resp,
        {
            .title = title,
            .session = session,
            .notifications = "",
        }
    );
    resp.content.append(javascript_code);
    end_ui_template(resp);
    return resp;
}

} // namespace web_server::web_worker
