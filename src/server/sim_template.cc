#include "sim_template.h"
#include "../include/string.h"
#include "../include/time.h"

namespace sim {

Template::Template(server::HttpResponse& resp, const std::string& title, const std::string& scripts,
			const std::string& styles) : resp_(&resp) {
		resp_->headers["Content-Type"] = "text/html; charset=utf-8";
		resp_->content = "";
		resp_->content.append("<!DOCTYPE html>\n"
			"<html lang=\"en\">\n"
				"<head>\n"
					"<meta charset=\"utf-8\">\n"
					"<title>").append(title).append("</title>\n"
					"<link rel=\"stylesheet\" href=\"/kit/styles.css\">\n"
					"<script src=\"/kit/jquery.js\"></script>\n"
					"<script src=\"/kit/scripts.js\"></script>\n"
					"<script>var start_time=").append(myto_string(microtime()/1000)).append(", load=-1, time_difference;</script>\n"
					"<link rel=\"shortcut icon\" href=\"/kit/img/favicon.png\">\n");

		if (scripts.size())
			resp_->content.append("<>").append(scripts).append("</script>\n");

		if (styles.size())
			resp_->content.append("<style>").append(styles).append("</style>\n");

		resp_->content.append("</head>\n"
				"<body>\n"
					"<div class=\"navbar\">\n"
						"<div class=\"navbar-body\">\n"
							"<a href=\"/\" class=\"brand\">SIM</a>\n"
							"<a href=\"/round.php\">Contests</a>\n"
							"<a href=\"/files/\">Files</a>\n"
							"<a href=\"/submissions/\">Submissions</a>\n"
							"<div style=\"float:right\">\n"
								"<span id=\"clock\">").append(date("%H:%M:%S")).append("</span><a href=\"/login.php\"><strong>Log in</strong></a>\n"
							"<a href=\"/register.php\">Register</a></div>\n"
						"</div>\n"
					"</div>\n"
					"<div class=\"body\">\n");
	}

} // namespace sim