#include "sim_session.h"
#include "sim_template.h"

#include "../simlib/include/time.h"

Sim::Template::Template(Sim& sim, const std::string& title,
	const std::string& styles, const std::string& scripts) : sim_(sim) {
	sim_.resp_.headers["Content-Type"] = "text/html; charset=utf-8";
	sim_.resp_.content = "";

	*this << "<!DOCTYPE html>\n"
		"<html lang=\"en\">\n"
			"<head>\n"
				"<meta charset=\"utf-8\">\n"
				"<title>"
		<< htmlSpecialChars(title) << "</title>\n"
				"<link rel=\"stylesheet\" href=\"/kit/styles.css\">\n"
				"<script src=\"/kit/jquery.js\"></script>\n"
				"<script src=\"/kit/scripts.js\"></script>\n"
				"<link rel=\"shortcut icon\" href=\"/kit/img/favicon.png\">\n";

	if (scripts.size())
		*this << "<script>" << scripts << "</script>\n";

	if (styles.size())
		*this << "<style>" << styles << "</style>\n";

	*this << "</head>\n"
			"<body>\n"
				"<div class=\"navbar\">\n"
					"<a href=\"/\" class=\"brand\">SIM</a>\n"
					"<a href=\"/c/\">Contests</a>\n"
					"<div class=\"rightbar\">\n"
						"<time id=\"clock\">" << date("%H:%M:%S")
							<< " UTC</time>";

	if (sim_.session->open() == Session::OK)
		*this << "<div class=\"dropdown\">\n"
				"<a class=\"user\"><strong>"
			<< htmlSpecialChars(sim_.session->username) << "</strong>"
				"<b class=\"caret\"></b></a>\n"
				"<ul>\n"
					"<a href=\"/u/" << sim_.session->user_id
						<< "\">Edit profile</a>\n"
					"<a href=\"/u/" << sim_.session->user_id
						<< "/change-password\">Change password</a>\n"
					"<a href=\"/logout\">Logout</a>\n"
				"</ul>\n"
				"</div>";

	else
		*this << "<a href=\"/login\"><strong>Log in</strong></a>\n"
			"<a href=\"/signup\">Sign up</a>";

	*this << "</div>\n"
		"</div>\n"
		"<div class=\"body\">\n";
}

Sim::Template::~Template() {
	*this << "</div>\n"
			"<script>var start_time="
		<< toString(microtime() / 1000) << ";</script>\n"
			"</body>\n"
			"</html>\n";
}
