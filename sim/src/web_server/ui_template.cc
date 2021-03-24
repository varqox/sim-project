#include "src/web_server/ui_template.hh"
#include "sim/sessions/session.hh"
#include "sim/users/user.hh"
#include "simlib/file_info.hh"
#include "simlib/string_view.hh"
#include "src/web_server/capabilities/contests.hh"
#include "src/web_server/capabilities/jobs.hh"
#include "src/web_server/capabilities/logs.hh"
#include "src/web_server/capabilities/problems.hh"
#include "src/web_server/capabilities/submissions.hh"
#include "src/web_server/capabilities/users.hh"
#include "src/web_server/http/response.hh"

#include <chrono>

using sim::users::User;
using web_server::http::Response;

namespace {

enum StaticFile {
    STYLES_CSS,
    JQUERY_JS,
    SCRIPTS_JS,
};

} // namespace

// Technique used to force browsers to always keep up-to-date version of the files below
template <StaticFile static_file>
static StringView get_hash_of() {
    static auto str = [] {
        auto path = [] {
            switch (static_file) {
            case STYLES_CSS: return "static/kit/styles.css";
            case JQUERY_JS: return "static/kit/jquery.js";
            case SCRIPTS_JS: return "static/kit/scripts.js";
            }
        }();
        return to_string(std::chrono::duration_cast<std::chrono::microseconds>(
                             get_modification_time(path).time_since_epoch())
                             .count());
    }();
    return str;
}

namespace web_server {

void begin_ui_template(Response& resp, UiTemplateParams params) {
    // Protect from clickjacking
    resp.headers["X-Frame-Options"] = "DENY";
    resp.headers["x-content-type-options"] = "nosniff";
    resp.headers["Content-Security-Policy"] = "default-src 'none'; "
                                              "style-src 'self' 'unsafe-inline'; "
                                              "script-src 'self' 'unsafe-inline'; "
                                              "connect-src 'self'; "
                                              "object-src 'self'; "
                                              "frame-src 'self'; "
                                              "img-src 'self'; ";

    // Disable X-XSS-Protection, as it may be used to misbehave the whole website
    resp.headers["X-XSS-Protection"] = "0";

    resp.headers["Content-Type"] = "text/html; charset=utf-8";

    // clang-format off
    resp.content.append("<!DOCTYPE html>"
        "<html lang=\"en\">"
            "<head>"
                "<meta charset=\"utf-8\">"
                "<title>", html_escape(params.title), "</title>"
                "<link rel=\"stylesheet\" type=\"text/css\" "
                      "href=\"/kit/styles.css?",
                          get_hash_of<STYLES_CSS>(), "\">"
                "<script src=\"/kit/jquery.js?",
                    get_hash_of<JQUERY_JS>(), "\"></script>"
                "<script src=\"/kit/scripts.js?",
                    get_hash_of<SCRIPTS_JS>(), "\"></script>"
                "<link rel=\"shortcut icon\" type=\"image/png\" "
                      "href=\"/kit/img/favicon.png\"/>");
    // clang-format on

    resp.content.append("</head><body><div class=\"navbar\">"
                        "<a href=\"/\" class=\"brand\">Sim beta</a>");

    if (capabilities::contests_for(params.session).web_ui_view) {
        // clang-format off
        resp.content.append("<script>"
                "var nav = document.querySelector('.navbar');"
                "nav.appendChild(a_view_button('/c', 'Contests', undefined, contest_chooser));"
                "nav.querySelector('script').remove()"
            "</script>");
        // clang-format on
    }
    if (capabilities::problems_for(params.session).web_ui_view) {
        resp.content.append("<a href=\"/p\">Problems</a>");
    }
    if (capabilities::users_for(params.session).web_ui_view) {
        resp.content.append("<a href=\"/u\">Users</a>");
    }
    if (capabilities::submissions_for(params.session).web_ui_view) {
        resp.content.append("<a href=\"/s\">Submissions</a>");
    }
    if (capabilities::jobs_for(params.session).web_ui_view) {
        resp.content.append("<a href=\"/jobs\">Job queue</a>");
    }
    if (capabilities::logs_for(params.session).view) {
        resp.content.append("<a href=\"/logs\">Logs</a>");
    }

    resp.content.append("<time id=\"clock\">", date("%H:%M:%S"), "<sup>UTC</sup></time>");

    if (params.session) {
        char utype_c = [&] {
            switch (params.session->user_type) {
            case User::Type::NORMAL: return 'N';
            case User::Type::TEACHER: return 'T';
            case User::Type::ADMIN: return 'A';
            }
            __builtin_unreachable();
        }();
        // clang-format off
        resp.content.append("<div class=\"dropmenu down\">"
                "<a class=\"user dropmenu-toggle\" user-type=\"", utype_c, "\">"
                    "<strong>", html_escape(params.session->username), "</strong>"
                "</a>"
                "<ul>"
                    "<a href=\"/u/", params.session->user_id, "\">My profile</a>"
                    "<a href=\"/u/", params.session->user_id, "/edit\">"
                        "Edit profile</a>"
                    "<a href=\"/u/", params.session->user_id, "/change-password\">"
                        "Change password</a>"
                    "<a href=\"/logout\">Logout</a>"
                "</ul>"
                "</div>");
        // clang-format on
    } else {
        resp.content.append("<a href=\"/login\"><strong>Log in</strong></a>"
                            "<a href=\"/signup\">Sign up</a>");
    }

    // clang-format off
    resp.content.append("</div>"
           "<div class=\"notifications\">", params.notifications, "</div>");
    // clang-format on
}

void end_ui_template(Response& resp) {
    // clang-format off
    resp.content.append("<script>"
            "var start_time=", microtime() / 1000, ";"
            "function update_clock() {"
                "var t = new Date();"
                "t.setTime(t.getTime() -"
                             "window.performance.timing.responseStart +"
                             "start_time);"
                "var h = t.getHours();"
                "var m = t.getMinutes();"
                "var s = t.getSeconds();"
                "h = (h < 10 ? '0' : '') + h;"
                "m = (m < 10 ? '0' : '') + m;"
                "s = (s < 10 ? '0' : '') + s;"
               // Update the displayed time
                "var tzo = -t.getTimezoneOffset();"
                "document.getElementById('clock').innerHTML = "
                   "String().concat(h, ':', m, ':', s, '<sup>UTC', (tzo >= 0 ? '+' : ''), tzo / 60, '</sup>');"
                "setTimeout(update_clock, 1000 - t.getMilliseconds());"
            "}"
            "update_clock();"
        "</script></body></html>");
    // clang-format on
}

} // namespace web_server
