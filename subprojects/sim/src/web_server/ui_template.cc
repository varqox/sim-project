#include "capabilities/contests.hh"
#include "capabilities/jobs.hh"
#include "capabilities/logs.hh"
#include "capabilities/problems.hh"
#include "capabilities/submissions.hh"
#include "capabilities/users.hh"
#include "http/response.hh"
#include "ui_template.hh"

#include <chrono>
#include <simlib/file_info.hh>
#include <simlib/json_str/json_str.hh>
#include <simlib/string_transform.hh>
#include <simlib/string_view.hh>
#include <simlib/time.hh>

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
                             get_modification_time(path).time_since_epoch()
        )
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
                    "href=\"/ui/", get_hash_of<STYLES_CSS>(), "/styles.css\">"
                "<script src=\"/ui/", get_hash_of<JQUERY_JS>(), "/jquery.js\"></script>"
                "<script src=\"/ui/", get_hash_of<SCRIPTS_JS>(), "/scripts.js\"></script>"
                "<link rel=\"shortcut icon\" type=\"image/x-icon\" href=\"/favicon.ico\"/>"
            "</head>"
        "<body>"
            "<script>"
                "sim_template(",
                    sim_template_params(params.session), ',',
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                    ).count(),
                ");"
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
        obj.prop_obj("logs", [&](auto& obj) {
            obj.prop("ui_view", capabilities::logs_for(session).view);
        });
        obj.prop_obj("problems", [&](auto& obj) {
            const auto caps = capabilities::problems(session);
            obj.prop("ui_view", caps.web_ui_view);
            obj.prop("add_problem", caps.add_problem);
            obj.prop(
                "add_problem_with_visibility_private", caps.add_problem_with_visibility_private
            );
            obj.prop(
                "add_problem_with_visibility_contest_only",
                caps.add_problem_with_visibility_contest_only
            );
            obj.prop("add_problem_with_visibility_public", caps.add_problem_with_visibility_public);
            auto fill_with_list_caps = [&](auto& obj,
                                           const capabilities::ProblemsListCapabilities caps) {
                obj.prop("query_all", caps.query_all);
                obj.prop("query_with_visibility_public", caps.query_with_visibility_public);
                obj.prop(
                    "query_with_visibility_contest_only", caps.query_with_visibility_contest_only
                );
                obj.prop("query_with_visibility_private", caps.query_with_visibility_private);
            };
            obj.prop_obj("list_all", [&](auto& obj) {
                fill_with_list_caps(obj, capabilities::list_problems(session));
            });
            if (session) {
                obj.prop_obj("list_my", [&](auto& obj) {
                    fill_with_list_caps(
                        obj, capabilities::list_user_problems(session, session->user_id)
                    );
                });
            }
        });
        obj.prop_obj("submissions", [&](auto& obj) {
            obj.prop("ui_view", capabilities::submissions(session).web_ui_view);
        });
        obj.prop_obj("users", [&](auto& obj) {
            const auto caps = capabilities::users(session);
            obj.prop("ui_view", caps.web_ui_view);
            obj.prop("add_user", caps.add_user);
            obj.prop("add_admin", caps.add_admin);
            obj.prop("add_teacher", caps.add_teacher);
            obj.prop("add_normal_user", caps.add_normal_user);
            obj.prop_obj("list_all", [&](auto& obj) {
                const auto caps = capabilities::list_users(session);
                obj.prop("query_all", caps.query_all);
                obj.prop("query_with_type_admin", caps.query_with_type_admin);
                obj.prop("query_with_type_teacher", caps.query_with_type_teacher);
                obj.prop("query_with_type_normal", caps.query_with_type_normal);
            });
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
