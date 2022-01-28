#include "src/web_server/ui_template.hh"
#include "sim/sessions/session.hh"
#include "sim/users/user.hh"
#include "simlib/concat_tostr.hh"
#include "simlib/file_info.hh"
#include "simlib/json_str/json_str.hh"
#include "simlib/string_transform.hh"
#include "simlib/string_view.hh"
#include "src/web_server/capabilities/contests.hh"
#include "src/web_server/capabilities/jobs.hh"
#include "src/web_server/capabilities/logs.hh"
#include "src/web_server/capabilities/problems.hh"
#include "src/web_server/capabilities/submissions.hh"
#include "src/web_server/capabilities/users.hh"
#include "src/web_server/http/response.hh"
#include "src/web_server/web_worker/web_worker.hh"

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
    resp.headers["Content-Security-Policy"] =
            "default-src 'none'; "
            "style-src 'self' 'unsafe-inline'; " // TODO: get rid of unsafe-inline (this
                                                 // requires CppSyntaxHighlighter to be
                                                 // implemented in some different way either in
                                                 // JS or styles in styles.css)
            "script-src 'self' 'unsafe-inline'; " // TODO: get rid of unsafe-inline (this
                                                  // requires no inline js, so url dispatch in
                                                  // UI is done from scripts.js and we provide
                                                  // required parameters (when generating
                                                  // ui_template response) not inside <script>
                                                  // tag but in some other way)
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
                      "href=\"/kit/img/favicon.png\"/>"
            "</head>"
        "<body>"
            "<script>"
                "sim_template(", sim_template_params(params.session), ',', microtime() / 1000, ");"
                "document.body.appendChild(elem_with_class('div', 'notifications')).innerHTML = '", params.notifications, "';");
    // clang-format on
}

void end_ui_template(Response& resp) { resp.content.append("</script></body></html>"); }

std::string sim_template_params(const decltype(web_worker::Context::session)& session) {
    json_str::Object obj;
    obj.prop_obj("capabilities", [&](auto& obj) {
        obj.prop_obj("contests", [&](auto& obj) {
            obj.prop("ui_view", capabilities::contests_for(session).web_ui_view);
        });
        obj.prop_obj("jobs", [&](auto& obj) {
            obj.prop("ui_view", capabilities::jobs_for(session).web_ui_view);
        });
        obj.prop_obj("logs",
                [&](auto& obj) { obj.prop("ui_view", capabilities::logs_for(session).view); });
        obj.prop_obj("problems", [&](auto& obj) {
            obj.prop("ui_view", capabilities::problems(session).web_ui_view);
        });
        obj.prop_obj("submissions", [&](auto& obj) {
            obj.prop("ui_view", capabilities::submissions_for(session).web_ui_view);
        });
        obj.prop_obj("users", [&](auto& obj) {
            auto caps = capabilities::users_for(session);
            obj.prop("ui_view", bool{caps.web_ui_view});
            obj.prop("add_user", bool{caps.add_user});
            obj.prop("add_admin", bool{caps.add_admin});
            obj.prop("add_teacher", bool{caps.add_teacher});
            obj.prop("add_normal_user", bool{caps.add_normal_user});
        });
        obj.prop_obj("contests", [&](auto& obj) {
            obj.prop("ui_view", capabilities::contests_for(session).web_ui_view);
        });
    });
    if (session) {
        obj.prop_obj("session", [&](auto& obj) {
            obj.prop("user_id", session->user_id);
            obj.prop("user_type", session->user_type);
            obj.prop("username", session->username);
        });
    } else {
        obj.prop("session", nullptr);
    }
    return std::move(obj).into_str();
}

} // namespace web_server
