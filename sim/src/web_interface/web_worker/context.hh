#pragma once

#include "sim/mysql.hh"
#include "sim/session.hh"
#include "simlib/string_view.hh"
#include "src/web_interface/http_cookies.hh"
#include "src/web_interface/http_request.hh"
#include "src/web_interface/http_response.hh"

namespace sim::web_worker {

struct Context {
    const server::HttpRequest& request;
    MySQL::Connection& mysql;

    struct Session {
        decltype(sim::Session::id) id;
        decltype(sim::Session::csrf_token) csrf_token;
        decltype(sim::Session::user_id) user_id;
        decltype(User::type) user_type;
        decltype(User::username) username;
        decltype(sim::Session::data) data;
        decltype(sim::Session::data) orig_data;
        static constexpr CStringView id_cookie_name = "session";
        static constexpr CStringView csrf_token_cookie_name = "csrf_token";
    };
    std::optional<Session> session;
    server::HttpCookies cookie_changes;

    void open_session();
    // void create_new_session(); // TODO: implement during refactor of log-in / sign-up
    // void destroy_session(); // TODO: implement during refactor of log-out
    void close_session();

    server::HttpResponse response(StringView status_code, StringView content);

    server::HttpResponse response_ok(StringView content = "") {
        return response("200 OK", content);
    }

    server::HttpResponse response_400(StringView content) {
        return response("400 Bad Request", content);
    }

    server::HttpResponse response_403(StringView content = "") {
        return response("403 Forbidden", content);
    }

    server::HttpResponse response_404(StringView content = "") {
        return response("404 Not Found", content);
    }
};

} // namespace sim::web_worker
