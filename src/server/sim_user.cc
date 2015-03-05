#include "sim.h"
#include "sim_session.h"
#include "sim_template.h"

#include "../include/debug.h"
#include "../include/memory.h"
#include "../include/sha.h"

#include <cppconn/prepared_statement.h>

using std::string;

void SIM::login() {
	if (req_->method == server::HttpRequest::POST) {
		// Try to login
		const std::map<string, string> &form = req_->form_data.other;
		std::map<string, string>::const_iterator username, password;

		// Check if all fields exist
		if ((username = form.find("username")) != form.end() &&
				(password = form.find("password")) != form.end()) {
			UniquePtr<sql::PreparedStatement> pstmt = db_conn_->mysql()
					->prepareStatement("SELECT id FROM `users` WHERE username=? AND password=?");
			pstmt->setString(1, username->second);
			pstmt->setString(2, sha256(password->second));
			pstmt->execute();

			UniquePtr<sql::ResultSet> res = pstmt->getResultSet();

			if (res->next()) {
				session->create(res->getString(1));
				resp_.status_code = "307 Temporary Redirect";
				resp_.headers["Location"] = "/";
				return;
			}
		}
	}
	Template templ(*this, "Login");
	templ << "<div class=\"form-container\">\n"
				"<h1>Log in</h1>\n"
				"<form method=\"post\">\n"
					"<label>Username</label>\n"
					"<input class=\"input-block\" type=\"text\" name=\"username\"/>\n"
					"<label>Password</label>\n"
					"<input class=\"input-block\" type=\"password\" name=\"password\"/>\n"
					"<input type=\"submit\" value=\"Log in\"/>\n"
				"</form>\n"
				"</div>\n";
}

void SIM::logout() {
	session->open();
	session->destroy();
	resp_.status_code = "307 Temporary Redirect";
	resp_.headers["Location"] = "/login";
}

void SIM::signUp() {
	if (session->open() == Session::OK) {
		resp_.status_code = "307 Temporary Redirect";
		resp_.headers["Location"] = "/";
		return;
	}

	string info, username, first_name, last_name, email, password1, password2;
	if (req_->method == server::HttpRequest::POST) {
		// Try to login
		const std::map<string, string> &form = req_->form_data.other;
		std::map<string, string>::const_iterator it;

		// Check if all fields exist
		if ((it = form.find("username")) == form.end() || it->second.empty())
			info += "<p>Username cannot be blank</p>";
		else
			username = it->second;

		if ((it = form.find("first_name")) == form.end() || it->second.empty())
			info += "<p>First name cannot be blank</p>";
		else
			first_name = it->second;

		if ((it = form.find("last_name")) == form.end() || it->second.empty())
			info += "<p>Last name cannot be blank</p>";
		else
			last_name = it->second;

		if ((it = form.find("email")) == form.end() || it->second.empty())
			info += "<p>Email cannot be blank</p>";
		else
			email = it->second;

		if ((it = form.find("password1")) == form.end() || (password1 = it->second,
				(it = form.find("password2")) == form.end()) || (password2 = it->second, password1 != password2))
			info += "<p>Passwords don't match</p>";

		// If all field are ok
		try {
			if (info.empty()) {
				UniquePtr<sql::PreparedStatement> pstmt = db_conn_->mysql()
						->prepareStatement("INSERT INTO `users` (username, first_name, last_name, email, password) SELECT ?, ?, ?, ?, ? FROM `users` WHERE NOT EXISTS (SELECT id FROM `users` WHERE username=?) LIMIT 1");
				pstmt->setString(1, username);
				pstmt->setString(2, first_name);
				pstmt->setString(3, last_name);
				pstmt->setString(4, email);
				pstmt->setString(5, sha256(password1));
				pstmt->setString(6, username);

				if (pstmt->executeUpdate() == 1) {
					pstmt.reset(db_conn_->mysql()->prepareStatement("SELECT id FROM `users` WHERE username=?"));

					pstmt->setString(1, username);
					pstmt->execute();

					UniquePtr<sql::ResultSet> res = pstmt->getResultSet();

					if (res->next()) {
						session->create(res->getString(1));
						resp_.status_code = "307 Temporary Redirect";
						resp_.headers["Location"] = "/";
						return;
					}
				} else
					info += "<p>Username taken</p>";
			}
		} catch (...) {
			E("Catched exception: %s:%d\n", __FILE__, __LINE__);
		}
	}
	Template templ(*this, "Register");
	templ << info << "<div class=\"form-container\">\n"
		"<h1>Register</h1>\n"
		"<form method=\"post\">\n"
			"<label>Username</label>\n"
			"<input class=\"input-block\" type=\"text\" value=\"" << username << "\" name=\"username\"/>\n"
			"<label>First name</label>\n"
			"<input class=\"input-block\" type=\"text\" value=\"" << first_name << "\" name=\"first_name\"/>\n"
			"<label>Last name</label>\n"
			"<input class=\"input-block\" type=\"text\" value=\"" << last_name << "\" name=\"last_name\"/>\n"
			"<label>Email</label>\n"
			"<input class=\"input-block\" type=\"text\" value=\"" << email << "\" name=\"email\"/>\n"
			"<label>Password</label>\n"
			"<input class=\"input-block\" type=\"password\" name=\"password1\"/>\n"
			"<label>Repeat password</label>\n"
			"<input class=\"input-block\" type=\"password\" name=\"password2\"/>\n"
			"<input type=\"submit\" value=\"Sign up\"/>\n"
		"</form>\n"
		"</div>\n";
}
