#include "template.h"

#include <simlib/time.h>

void Template::baseTemplate(const StringView& title, const StringView& styles,
	const StringView& scripts)
{
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
				"<link rel=\"stylesheet\" href=\"/kit/styles.css\">"
				"<script src=\"/kit/jquery.js\"></script>"
				"<script src=\"/kit/scripts.js\"></script>"
				"<link rel=\"shortcut icon\" href=\"/kit/img/favicon.png\">");

	if (scripts.size())
		append("<script>", scripts, "</script>");

	if (styles.size())
		append("<style>", styles, "</style>");

	append("</head>"
			"<body>"
				"<div class=\"navbar\">"
					"<div>"
						"<a href=\"/\" class=\"brand\">SIM</a>"
						"<a href=\"/c/\">Contests</a>");

	if (Session::open() && Session::user_type < UTYPE_NORMAL) {
		append("<a href=\"/p\">Problemset</a>"
			"<a href=\"/u\">Users</a>");
		if (Session::user_type == UTYPE_ADMIN)
			append("<a href=\"/logs\">Logs</a>");
	}

	append("</div>"
			"<div class=\"rightbar\">"
				"<time id=\"clock\">", date("%H:%M:%S"),
					"<sup>UTC+0</sup></time>");

	if (Session::isOpen())
		append("<div class=\"dropmenu down\">"
				"<a class=\"user dropmenu-toggle\">"
					"<strong>", htmlEscape(Session::username), "</strong>"
				"</a>"
				"<ul>"
					"<a href=\"/u/", Session::user_id, "\">My profile</a>"
					"<a href=\"/u/", Session::user_id, "/submissions\">"
						"My submissions</a>"
					"<a href=\"/u/", Session::user_id, "/edit\">"
						"Edit profile</a>"
					"<a href=\"/u/", Session::user_id, "/change-password\">"
						"Change password</a>"
					"<a href=\"/logout\">Logout</a>"
				"</ul>"
				"</div>");

	else
		append("<a href=\"/login\"><strong>Log in</strong></a>"
			"<a href=\"/signup\">Sign up</a>");

	append("</div>"
		"</div>");

	template_began = true;
}

void Template::endTemplate() {
	if (template_began) {
		template_began = false;
		append("<script>var start_time=", toString(microtime() / 1000),
					";</script>"
				"</body>"
			"</html>");
	}
}
