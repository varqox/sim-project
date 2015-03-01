#include "sim_template.h"

#include "../include/time.h"

#include <cstring>

SIM::Template::Template(server::HttpResponse& resp, const std::string& title, const std::string& scripts,
			const std::string& styles) : resp_(&resp) {
	resp_->headers["Content-Type"] = "text/html; charset=utf-8";
	resp_->content = "";
	*this << "<!DOCTYPE html>\n"
		"<html lang=\"en\">\n"
			"<head>\n"
				"<meta charset=\"utf-8\">\n"
				"<title>"
		<< title << "</title>\n"
				"<link rel=\"stylesheet\" href=\"/kit/styles.css\">\n"
				"<script src=\"/kit/jquery.js\"></script>\n"
				"<script src=\"/kit/scripts.js\"></script>\n"
				"<script>var start_time="
		<< myto_string(microtime()/1000) << ", load=-1, time_difference;</script>\n"
				"<link rel=\"shortcut icon\" href=\"/kit/img/favicon.png\">\n";

	if (scripts.size())
		*this << "<script>" << scripts << "</script>\n";

	if (styles.size())
		*this << "<style>" << styles << "</style>\n";

	*this << "</head>\n"
			"<body>\n"
				"<div class=\"navbar\">\n"
					"<div class=\"navbar-body\">\n"
						"<a href=\"/\" class=\"brand\">SIM</a>\n"
						"<a href=\"/round\">Contests</a>\n"
						"<a href=\"/files/\">Files</a>\n"
						"<a href=\"/submissions/\">Submissions</a>\n"
						"<div style=\"float:right\">\n"
							"<span id=\"clock\">"
		<< date("%H:%M:%S") << "</span><a href=\"/login\"><strong>Log in</strong></a>\n"
						"<a href=\"/register\">Register</a></div>\n"
					"</div>\n"
				"</div>\n"
				"<div class=\"body\">\n";
}
