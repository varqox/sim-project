#include "sim.h"
#include "sim_session.h"
#include "sim_template.h"

#include "../include/debug.h"
#include "../include/memory.h"
#include "../include/sha.h"

#include <cppconn/prepared_statement.h>

using std::string;
using std::map;

void SIM::login() {
	if (req_->method == server::HttpRequest::POST) {
		// Try to login
		const map<string, string> &form = req_->form_data.other;
		map<string, string>::const_iterator username, password;

		// Check if all fields exist
		if ((username = form.find("username")) != form.end() &&
				(password = form.find("password")) != form.end()) {
			try {
				UniquePtr<sql::PreparedStatement> pstmt(db_conn()
						->prepareStatement("SELECT id FROM `users` WHERE username=? AND password=?"));
				pstmt->setString(1, username->second);
				pstmt->setString(2, sha256(password->second));
				pstmt->execute();

				UniquePtr<sql::ResultSet> res(pstmt->getResultSet());

				if (res->next()) {
					// Delete old sessions
					session->open();
					session->destroy();
					// Create new
					session->create(res->getString(1));
					if (req_->target.size() > 6)
						return redirect(req_->target.substr(6));
					return redirect("/");
				}
			} catch(...) {
				E("\e[31mCaught exception: %s:%d\e[0m\n", __FILE__, __LINE__);
			}
		}
	}
	Template templ(*this, "Login");
	templ << "<div class=\"form-container\">\n"
				"<h1>Log in</h1>\n"
				"<form method=\"post\">\n"
					// Username
					"<div class=\"field-group\">\n"
						"<label>Username</label>\n"
						"<input type=\"text\" name=\"username\">\n"
					"</div>\n"
					// Password
					"<div class=\"field-group\">\n"
						"<label>Password</label>\n"
						"<input type=\"password\" name=\"password\">\n"
					"</div>\n"
					"<input type=\"submit\" value=\"Log in\">\n"
				"</form>\n"
				"</div>\n";
}

void SIM::logout() {
	session->open();
	session->destroy();
	redirect("/login");
}

void SIM::signUp() {
	if (session->open() == Session::OK)
		return redirect("/");

	string info, username, first_name, last_name, email, password1, password2;
	if (req_->method == server::HttpRequest::POST) {
		// Try to login
		const map<string, string> &form = req_->form_data.other;
		map<string, string>::const_iterator it;

		// Validate all fields
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

		// If all fields are ok
		if (info.empty())
			try {
				UniquePtr<sql::PreparedStatement> pstmt(db_conn()
						->prepareStatement("INSERT IGNORE INTO `users` (username, first_name, last_name, email, password) VALUES(?, ?, ?, ?, ?)"));
				pstmt->setString(1, username);
				pstmt->setString(2, first_name);
				pstmt->setString(3, last_name);
				pstmt->setString(4, email);
				pstmt->setString(5, sha256(password1));

				if (pstmt->executeUpdate() == 1) {
					pstmt.reset(db_conn()->prepareStatement("SELECT id FROM `users` WHERE username=?"));

					pstmt->setString(1, username);
					pstmt->execute();

					UniquePtr<sql::ResultSet> res(pstmt->getResultSet());

					if (res->next()) {
						session->create(res->getString(1));
						return redirect("/");
					}
				} else
					info += "<p>Username taken</p>";
			} catch (...) {
				E("\e[31mCaught exception: %s:%d\e[0m\n", __FILE__, __LINE__);
			}
	}
	Template templ(*this, "Register");
	templ << info << "<div class=\"form-container\">\n"
		"<h1>Register</h1>\n"
		"<form method=\"post\">\n"
			// Username
			"<div class=\"field-group\">\n"
				"<label>Username</label>\n"
				"<input type=\"text\" name=\"username\" value=\""
					<< htmlSpecialChars(username) << "\">\n"
			"</div>\n"
			// First Name
			"<div class=\"field-group\">\n"
				"<label>First name</label>\n"
				"<input type=\"text\" name=\"first_name\" value=\""
					<< htmlSpecialChars(first_name) << "\">\n"
			"</div>\n"
			// Last name
			"<div class=\"field-group\">\n"
				"<label>Last name</label>\n"
				"<input type=\"text\" name=\"last_name\" value=\""
					<< htmlSpecialChars(last_name) << "\">\n"
			"</div>\n"
			// Email
			"<div class=\"field-group\">\n"
				"<label>Email</label>\n"
				"<input type=\"email\" name=\"email\" value=\""
					<< htmlSpecialChars(email) << "\">\n"
			"</div>\n"
			// Password
			"<div class=\"field-group\">\n"
				"<label>Password</label>\n"
				"<input type=\"password\" name=\"password1\">\n"
			"</div>\n"
			// Repeat password
			"<div class=\"field-group\">\n"
				"<label>Repeat password</label>\n"
				"<input type=\"password\" name=\"password2\">\n"
			"</div>\n"
			"<input type=\"submit\" value=\"Sign up\">\n"
		"</form>\n"
		"</div>\n";
}
