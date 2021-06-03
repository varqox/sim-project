#include "src/web_server/ui_template.hh"
#include "sim/sessions/session.hh"
#include "sim/users/user.hh"
#include "simlib/file_info.hh"
#include "simlib/string_transform.hh"
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
                "<script>",
                    "const logged_user_id = ",
                        params.session ? to_string(params.session->user_id) : StaticCStringBuff{"null"}, ";"
                    "const logged_user_type = ", params.session ? params.session->user_type.to_enum().to_quoted_str() : "null", ";"
                    "const logged_user_username = ", params.session ? json_stringify(params.session->username) : "null", ";"
                "</script>"
                "<script src=\"/kit/jquery.js?",
                    get_hash_of<JQUERY_JS>(), "\"></script>"
                "<script src=\"/kit/scripts.js?",
                    get_hash_of<SCRIPTS_JS>(), "\"></script>"
                "<link rel=\"shortcut icon\" type=\"image/png\" "
                      "href=\"/kit/img/favicon.png\"/>"
            "</head>"
        "<body>"
            "<script>"
                "sim_template({"
                    "contests:", capabilities::contests_for(params.session).web_ui_view, ","
                    "jobs:", capabilities::jobs_for(params.session).web_ui_view, ","
                    "logs:", capabilities::logs_for(params.session).view, ","
                    "problems:", capabilities::problems_for(params.session).web_ui_view, ","
                    "submissions:", capabilities::submissions_for(params.session).web_ui_view, ","
                    "users:", capabilities::users_for(params.session).web_ui_view, ","
                "},", microtime() / 1000, ");"
                "document.body.appendChild(elem_with_class('div', 'notifications')).innerHTML = '", params.notifications, "';");
    // clang-format on
}

void end_ui_template(Response& resp) { resp.content.append("</script></body></html>"); }

} // namespace web_server
