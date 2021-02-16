#include "simlib/file_info.hh"
#include "src/web_interface/sim.hh"

#include <chrono>

using sim::User;

namespace {

enum StaticFile {
    STYLES_CSS,
    JQUERY_JS,
    SCRIPTS_JS,
};

} // namespace

// Technique used to force browsers to always keep up-to-date version of
// the files below
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

void Sim::page_template(StringView title, StringView styles, StringView scripts) {
    STACK_UNWINDING_MARK;

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

    resp.headers["X-XSS-Protection"] =
        "0"; // Disable this, as it may be used to misbehave the whole website

    resp.headers["Content-Type"] = "text/html; charset=utf-8";
    resp.content = "";

    // clang-format off
	append("<!DOCTYPE html>"
	    "<html lang=\"en\">"
	        "<head>"
	            "<meta charset=\"utf-8\">"
	            "<title>", html_escape(title), "</title>"
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

    if (!scripts.empty()) {
        append("<script>", scripts, "</script>");
    }

    if (!styles.empty()) {
        append("<style>", styles, "</style>");
    }

    // clang-format off
	append("</head>"
	        "<body>"
	            "<div class=\"navbar\">"
	                "<a href=\"/\" class=\"brand\">Sim beta</a>"
	                "<script>"
	                    "var nav = document.querySelector('.navbar');"
	                    "nav.appendChild(a_view_button('/c', 'Contests',"
	                                                  "undefined,"
	                                                  "contest_chooser));"
	                    "nav.querySelector('script').remove()"
	                "</script>"
	                "<a href=\"/p\">Problems</a>");
    // clang-format on

    if (session_is_open) {
        if (uint(users_get_overall_permissions() & UserPermissions::VIEW_ALL)) {
            append("<a href=\"/u\">Users</a>");
        }

        if (uint(submissions_get_overall_permissions() & SubmissionPermissions::VIEW_ALL)) {
            append("<a href=\"/s\">Submissions</a>");
        }

        if (uint(jobs_get_overall_permissions() & JobPermissions::VIEW_ALL)) {
            append("<a href=\"/jobs\">Job queue</a>");
        }

        if (session_user_type == User::Type::ADMIN) {
            append("<a href=\"/logs\">Logs</a>");
        }
    }

    append("<time id=\"clock\">", date("%H:%M:%S"), "<sup>UTC</sup></time>");

    if (session_is_open) {
        char utype_c = '?';
        switch (session_user_type) {
        case User::Type::NORMAL: utype_c = 'N'; break;
        case User::Type::TEACHER: utype_c = 'T'; break;
        case User::Type::ADMIN: utype_c = 'A'; break;
        }

        // clang-format off
        append("<div class=\"dropmenu down\">"
                "<a class=\"user dropmenu-toggle\" user-type=\"", utype_c, "\">"
                    "<strong>", html_escape(session_username), "</strong>"
                "</a>"
                "<ul>"
                    "<a href=\"/u/", session_user_id, "\">My profile</a>"
                    "<a href=\"/u/", session_user_id, "/edit\">"
                        "Edit profile</a>"
                    "<a href=\"/u/", session_user_id, "/change-password\">"
                        "Change password</a>"
                    "<a href=\"/logout\">Logout</a>"
                "</ul>"
                "</div>");
        // clang-format on

    } else {
        append("<a href=\"/login\"><strong>Log in</strong></a>"
               "<a href=\"/signup\">Sign up</a>");
    }

    // clang-format off
	append("</div>"
	       "<div class=\"notifications\">", notifications, "</div>");
    // clang-format on

#ifdef DEBUG
    notifications.clear();
#endif

    page_template_began = true;
}

void Sim::page_template_end() {
    STACK_UNWINDING_MARK;

    if (page_template_began) {
        page_template_began = false;
        // clang-format off
		append("<script>var start_time=", microtime() / 1000, ";"
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
}

void Sim::error_page_template(StringView status, StringView code, StringView message) {
    STACK_UNWINDING_MARK;

    resp.status_code = status.to_string();
    resp.headers.clear();

    auto prev = request.headers.get("Referer");
    if (prev.empty()) {
        prev = "/";
    }

    page_template(status);
    // clang-format off
	append("<center>"
	       "<h1 style=\"font-size:25px;font-weight:normal;\">",
	          code, " &mdash; ", message, "</h1>"
	       "<a class=\"btn\" href=\"", prev, "\">Go back</a>"
	       "</center>");
    // clang-format on
}
