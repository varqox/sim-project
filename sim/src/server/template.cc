#include "template.h"

#include <simlib/time.h>

Template::TemplateEnder Template::baseTemplate(const StringView& title,
	const StringView& styles, const StringView& scripts)
{
	resp.headers["Content-Type"] = "text/html; charset=utf-8";
	resp.content = "";

	append("<!DOCTYPE html>\n"
		"<html lang=\"en\">\n"
			"<head>\n"
				"<meta charset=\"utf-8\">\n"
				"<title>", htmlSpecialChars(title), "</title>\n"
				"<link rel=\"stylesheet\" href=\"/kit/styles.css\">\n"
				"<script src=\"/kit/jquery.js\"></script>\n"
				"<script src=\"/kit/scripts.js\"></script>\n"
				"<link rel=\"shortcut icon\" href=\"/kit/img/favicon.png\">\n");

	if (scripts.size())
		append("<script>", scripts, "</script>\n");

	if (styles.size())
		append("<style>", styles, "</style>\n");

	append("</head>\n"
			"<body>\n"
				"<div class=\"navbar\">\n"
					"<a href=\"/\" class=\"brand\">SIM</a>\n"
					"<a href=\"/c/\">Contests</a>\n");

	if (Session::open() && Session::user_type < UTYPE_NORMAL) {
		append("<a href=\"/u\">Users</a>\n");
		if (Session::user_type == UTYPE_ADMIN)
			append("<a href=\"/logs\">Logs</a>\n");
	}

	append("<div class=\"rightbar\">\n"
						"<time id=\"clock\">", date("%H:%M:%S"),
							" UTC</time>");

	if (Session::isOpen())
		append("<div class=\"dropdown\">\n"
				"<a class=\"user dropdown-toggle\">"
					"<strong>", htmlSpecialChars(Session::username), "</strong>"
				"</a>\n"
				"<ul>\n"
					"<a href=\"/u/", Session::user_id, "\">Edit profile</a>\n"
					"<a href=\"/u/", Session::user_id,
						"/change-password\">Change password</a>\n"
					"<a href=\"/logout\">Logout</a>\n"
				"</ul>\n"
				"</div>");

	else
		append("<a href=\"/login\"><strong>Log in</strong></a>\n"
			"<a href=\"/signup\">Sign up</a>");

	append("</div>\n"
		"</div>\n"
		"<div class=\"body\">\n");

	return TemplateEnder(*this);
}

void Template::endTemplate() {
	append("</div>\n"
			"<script>var start_time=", toString(microtime() / 1000),
				";</script>\n"
			"</body>\n"
		"</html>\n");
}
