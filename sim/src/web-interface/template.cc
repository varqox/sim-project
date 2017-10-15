#include "sim.h"

#ifndef STYLES_CSS_HASH
# define STYLES_CSS_HASH ""
#endif
#ifndef JQUERY_JS_HASH
# define JQUERY_JS_HASH ""
#endif
#ifndef SCRIPTS_JS_HASH
# define SCRIPTS_JS_HASH ""
#endif

void Sim::page_template(StringView title, StringView styles, StringView scripts)
{
	STACK_UNWINDING_MARK;

	// Protect from clickjacking
	resp.headers["X-Frame-Options"] = "DENY";
	resp.headers["Content-Security-Policy"] = "frame-ancestors 'none'";
	// Protect from XSS attack
	resp.headers["X-XSS-Protection"] = "1; mode=block";

	resp.headers["Content-Type"] = "text/html; charset=utf-8";
	resp.content = "";

	append("<!DOCTYPE html>"
		"<html lang=\"en\">"
			"<head>"
				"<meta charset=\"utf-8\">"
				"<title>", htmlEscape(title), "</title>"
				"<link rel=\"stylesheet\" type=\"text/css\""
					" href=\"/kit/styles.css?" STYLES_CSS_HASH "\">"
				"<script src=\"/kit/jquery.js?" JQUERY_JS_HASH "\"></script>"
				"<script src=\"/kit/scripts.js?" SCRIPTS_JS_HASH "\"></script>"
				"<link rel=\"shortcut icon\" href=\"/kit/img/favicon.png\">");

	if (scripts.size())
		append("<script>", scripts, "</script>");

	if (styles.size())
		append("<style>", styles, "</style>");

	append("</head>"
			"<body>"
				"<div class=\"navbar\">"
					"<div>"
						"<a href=\"/\" class=\"brand\">SIM beta</a>"
						"<script>"
							"a_preview_button('/c', 'Contests', undefined,"
								"contest_chooser).appendTo('.navbar > div');"
							"$('.navbar > div > script').remove();"
						"</script>"
						"<a href=\"/p\">Problems</a>");

	if (session_open()) {
		if (uint(users_get_permissions() & UserPermissions::VIEW_ALL))
			append("<a href=\"/u\">Users</a>");

		if (uint(submissions_get_permissions() &
			SubmissionPermissions::VIEW_ALL))
		{
			append("<a href=\"/s\">Submissions</a>");
		}

		if (uint(jobs_get_permissions() & JobPermissions::VIEW_ALL))
			append("<a href=\"/jobs/\">Job queue</a>");

		if (session_user_type == UserType::ADMIN)
			append("<a href=\"/logs\">Logs</a>");
	}

	append("</div>"
			"<div class=\"rightbar\">"
				"<time id=\"clock\">", date("%H:%M:%S"),
					"<sup>UTC</sup></time>");

	if (session_is_open)
		append("<div class=\"dropmenu down\">"
				"<a class=\"user dropmenu-toggle\">"
					"<strong>", htmlEscape(session_username), "</strong>"
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

	else
		append("<a href=\"/login\"><strong>Log in</strong></a>"
			"<a href=\"/signup\">Sign up</a>");

	append("</div>"
		"</div>"
		"<div class=\"notifications\">", notifications, "</div>");

	page_template_began = true;
}

void Sim::page_template_end() {
	STACK_UNWINDING_MARK;

	if (page_template_began) {
		page_template_began = false;
		append("<script>var start_time=", microtime() / 1000,
					";</script>"
				"</body>"
			"</html>");
	}
}

void Sim::error_page_template(StringView status, StringView code,
	StringView message)
{
	STACK_UNWINDING_MARK;

	resp.status_code = status.to_string();
	resp.headers.clear();

	auto prev = request.headers.get("Referer");
	if (prev.empty())
		prev = "/";

	page_template(status);
	append("<center>"
		"<h1 style=\"font-size:25px;font-weight:normal;\">", code, " &mdash; ",
			message, "</h1>"
		"<a class=\"btn\" href=\"", prev, "\">Go back</a>"
		"</center>");
}
