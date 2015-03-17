#include "sim_template.h"
#include "sim_session.h"

#include "../include/debug.h"
#include "../include/time.h"
#include "../include/memory.h"

#include <cppconn/prepared_statement.h>
#include <cstring>

SIM::Template::Template(SIM& sim, const std::string& title,
	const std::string& styles, const std::string& scripts) : sim_(sim) {
	sim_.resp_.headers["Content-Type"] = "text/html; charset=utf-8";
	sim_.resp_.content = "";
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
					"<a href=\"/\" class=\"brand\">SIM</a>\n"
					"<a href=\"/c/\">Contests</a>\n"
					"<div class=\"rightbar\">\n"
						"<span id=\"clock\">" << date("%H:%M:%S") << "</span>";
	if (sim_.session->open() == Session::OK) {
		try {
			UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()
					->prepareStatement("SELECT username FROM `users` WHERE id=?"));
			pstmt->setString(1, sim_.session->user_id);
			pstmt->execute();

			UniquePtr<sql::ResultSet> res(pstmt->getResultSet());

			if (res->next()) {
				*this << "<div class=\"dropdown\">\n"
						"<a href=\"#\" class=\"user\"><strong>"
					<< res->getString(1) << "</strong><b class=\"caret\"></b></a>\n"
						"<ul>\n"
						"<a href=\"/logout\">logout</a>\n"
						"</ul>\n"
						"</div>";
			} else {
				sim_.session->destroy();
				goto else_label;
			}
		} catch (...) {
			sim_.session->destroy();
			E("\e[31mCaught exception: %s:%d\e[0m\n", __FILE__, __LINE__);
		}
	} else {
	else_label:
		*this << "<a href=\"/login\"><strong>Log in</strong></a>\n"
			"<a href=\"/signup\">Sign up</a>";
	}
	*this << "</div>\n"
		"</div>\n"
		"<div class=\"body\">\n";
}
