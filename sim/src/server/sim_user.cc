#include "form_validator.h"
#include "sim_user.h"
#include "sim_session.h"
#include "sim_template.h"

#include "../include/debug.h"
#include "../include/sha.h"

#include <cppconn/prepared_statement.h>

using std::string;
using std::map;

void Sim::User::handle() {
	userProfile();
}

void Sim::User::login() {
	FormValidator fv(sim_.req_->form_data);
	string username;

	if (sim_.req_->method == server::HttpRequest::POST) {
		// Try to login
		string password;
		// Validate all fields
		fv.validate(username, "username", "Username", 30);
		fv.validate(password, "password", "Password");

		if (fv.noErrors())
			try {
				UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
					prepareStatement("SELECT id FROM `users` WHERE username=? AND password=?"));
				pstmt->setString(1, username);
				pstmt->setString(2, sha256(password));

				UniquePtr<sql::ResultSet> res(pstmt->executeQuery());

				if (res->next()) {
					// Delete old sessions
					sim_.session->open();
					sim_.session->destroy();
					// Create new
					sim_.session->create(res->getString(1));

					// If there is redirection string, redirect
					if (sim_.req_->target.size() > 6)
						return sim_.redirect(sim_.req_->target.substr(6));

					return sim_.redirect("/");
				}

			} catch (const std::exception& e) {
				E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__,
					__LINE__, e.what());

			} catch (...) {
				E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
			}
	}

	Template templ(sim_, "Login");
	templ << fv.errors() << "<div class=\"form-container\">\n"
				"<h1>Log in</h1>\n"
				"<form method=\"post\">\n"
					// Username
					"<div class=\"field-group\">\n"
						"<label>Username</label>\n"
						"<input type=\"text\" name=\"username\" value=\""
							<< htmlSpecialChars(username) << "\" size=\"24\" maxlength=\"30\" required>\n"
					"</div>\n"
					// Password
					"<div class=\"field-group\">\n"
						"<label>Password</label>\n"
						"<input type=\"password\" name=\"password\" size=\"24\">\n"
					"</div>\n"
					"<input class=\"btn\" type=\"submit\" value=\"Log in\">\n"
				"</form>\n"
				"</div>\n";
}

void Sim::User::logout() {
	sim_.session->open();
	sim_.session->destroy();
	sim_.redirect("/login");
}

void Sim::User::signUp() {
	if (sim_.session->open() == Session::OK)
		return sim_.redirect("/");

	FormValidator fv(sim_.req_->form_data);
	string username, first_name, last_name, email, password1, password2;

	if (sim_.req_->method == server::HttpRequest::POST) {
		// Validate all fields
		fv.validateNotBlank(username, "username", "Username", 30);
		fv.validateNotBlank(first_name, "first_name", "First Name");
		fv.validateNotBlank(last_name, "last_name", "Last Name", 60);
		fv.validateNotBlank(email, "email", "Email", 60);
		if (fv.validate(password1, "password1", "Password") &&
				fv.validate(password2, "password2", "Password (repeat)") &&
				password1 != password2)
			fv.addError("Passwords don't match");

		// If all fields are ok
		if (fv.noErrors())
			try {
				UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
						prepareStatement("INSERT IGNORE INTO `users` (username, first_name, last_name, email, password) VALUES(?, ?, ?, ?, ?)"));
				pstmt->setString(1, username);
				pstmt->setString(2, first_name);
				pstmt->setString(3, last_name);
				pstmt->setString(4, email);
				pstmt->setString(5, sha256(password1));

				if (pstmt->executeUpdate() == 1) {
					pstmt.reset(sim_.db_conn()->prepareStatement("SELECT id FROM `users` WHERE username=?"));
					pstmt->setString(1, username);

					UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
					if (res->next()) {
						sim_.session->create(res->getString(1));
						return sim_.redirect("/");
					}

				} else
					fv.addError("Username taken");

			} catch (const std::exception& e) {
				E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__,
					__LINE__, e.what());

			} catch (...) {
				E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
			}
	}

	Template templ(sim_, "Register");
	templ << fv.errors() << "<div class=\"form-container\">\n"
		"<h1>Register</h1>\n"
		"<form method=\"post\">\n"
			// Username
			"<div class=\"field-group\">\n"
				"<label>Username</label>\n"
				"<input type=\"text\" name=\"username\" value=\""
					<< htmlSpecialChars(username) << "\" size=\"24\" maxlength=\"30\" required>\n"
			"</div>\n"
			// First Name
			"<div class=\"field-group\">\n"
				"<label>First name</label>\n"
				"<input type=\"text\" name=\"first_name\" value=\""
					<< htmlSpecialChars(first_name) << "\" size=\"24\" maxlength=\"60\" required>\n"
			"</div>\n"
			// Last name
			"<div class=\"field-group\">\n"
				"<label>Last name</label>\n"
				"<input type=\"text\" name=\"last_name\" value=\""
					<< htmlSpecialChars(last_name) << "\" size=\"24\" maxlength=\"60\" required>\n"
			"</div>\n"
			// Email
			"<div class=\"field-group\">\n"
				"<label>Email</label>\n"
				"<input type=\"email\" name=\"email\" value=\""
					<< htmlSpecialChars(email) << "\" size=\"24\" maxlength=\"60\" required>\n"
			"</div>\n"
			// Password
			"<div class=\"field-group\">\n"
				"<label>Password</label>\n"
				"<input type=\"password\" name=\"password1\" size=\"24\">\n"
			"</div>\n"
			// Password (repeat)
			"<div class=\"field-group\">\n"
				"<label>password (repeat)</label>\n"
				"<input type=\"password\" name=\"password2\" size=\"24\">\n"
			"</div>\n"
			"<input class=\"btn\" type=\"submit\" value=\"Sign up\">\n"
		"</form>\n"
		"</div>\n";
}

void Sim::User::userProfile() {
	if (sim_.session->open() != Session::OK)
		return sim_.redirect("/login" + sim_.req_->target);

	size_t arg_beg = 3;

	// Extract user id
	string user_id;
	{
		int res_code = strtonum(user_id, sim_.req_->target, arg_beg,
				find(sim_.req_->target, '/', arg_beg));
		if (res_code == -1)
			return sim_.error404();

		arg_beg += res_code + 1;
	}

	// Get viewer rank and user information
	int user_rank, viewer_rank = sim_.getUserRank(sim_.session->user_id);
	string username, first_name, last_name, email;
	UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->prepareStatement(
		"SELECT username, first_name, last_name, email, type FROM users WHERE id=?"));
	pstmt->setString(1, user_id);

	UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
	if (res->next()) {
		username = res->getString(1);
		first_name = res->getString(2);
		last_name = res->getString(3);
		email = res->getString(4);
		user_rank = userTypeToRank(res->getString(5));
	} else
		return sim_.error404();

	enum ViewType { FULL, READ_ONLY } view_type = FULL;
	/* Check permissions
	* +---------------+---------+-------+---------+--------+
	* | user \ viewer | id == 1 | admin | teacher | normal |
	* +---------------+---------+-------+---------+--------+
	* |    normal     |  FULL   | FULL  |   RDO   |  ---   |
	* |    teacher    |  FULL   | FULL  |   ---   |  ---   |
	* |     admin     |  FULL   |  RDO  |   ---   |  ---   |
	* +---------------+---------+-------+---------+--------+
	*/
	if (user_id == sim_.session->user_id) {}

	else if (viewer_rank >= 2 || (viewer_rank == 1 && user_rank < 2))
		return sim_.error403();

	else if (viewer_rank == 1 ||
				(viewer_rank == 0 && sim_.session->user_id != "1"))
		view_type = READ_ONLY;

	FormValidator fv(sim_.req_->form_data);

	// TODO: separate editing and deleting
	// Delete account
	if (compareTo(sim_.req_->target, arg_beg, '/', "delete") == 0) {
		// Deleting "root" account (id 1) is forbidden
		if (user_id == "1") {
			Template templ(sim_, "Delete account");
			templ << "<h1>You cannot delete SIM root account</h1>";
			return;
		}

		if (view_type == READ_ONLY)
			return sim_.error403();

		if (sim_.req_->method == server::HttpRequest::POST)
			if (fv.exist("delete"))
				try {
					// Change contests and problems owner id to 1
					pstmt.reset(sim_.db_conn()->prepareStatement(
						"UPDATE rounds r, problems p "
						"SET r.owner=1, p.owner=1 "
						"WHERE r.owner=? OR p.owner=?"));
					pstmt->setString(1, user_id);
					pstmt->setString(2, user_id);
					pstmt->executeUpdate();

					// Delete submissions
					pstmt.reset(sim_.db_conn()->prepareStatement(
						"DELETE FROM submissions, submissions_to_rounds "
						"USING submissions INNER JOIN submissions_to_rounds "
						"WHERE submissions.user_id=? AND id=submission_id"));
					pstmt->setString(1, user_id);
					pstmt->executeUpdate();

					// Delete from users_to_contests
					pstmt.reset(sim_.db_conn()->prepareStatement(
						"DELETE FROM users_to_contests WHERE user_id=?"));
					pstmt->setString(1, user_id);
					pstmt->executeUpdate();

					// Delete user
					pstmt.reset(sim_.db_conn()->prepareStatement(
						"DELETE FROM users WHERE id=?"));
					pstmt->setString(1, user_id);

					if (pstmt->executeUpdate() > 0) {
						if (user_id == sim_.session->user_id)
							sim_.session->destroy();
						return sim_.redirect("/");
					}

				} catch (const std::exception& e) {
					E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__,
						__LINE__, e.what());

				} catch (...) {
					E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__,
						__LINE__);
				}

		Template templ(sim_, "Delete account");
		templ << fv.errors() << "<div class=\"form-container\">\n"
				"<h1>Delete account</h1>\n"
				"<form method=\"post\">\n"
					"<div class=\"field-group\">\n"
						"<label>Are you sure to delete account <a href=\"/u/"
							<< user_id << "\">"
							<< htmlSpecialChars(username) << "</a>, all its "
							"submissions and change owner of its contests and "
							"problems to SIM root?</label>\n"
					"</div>\n"
					"<div class=\"submit-yes-no\">\n"
						"<button class=\"btn-danger\" type=\"submit\" "
							"name=\"delete\">Yes, I'm sure</button>\n"
						"<a class=\"btn\" href=\"/u/" << user_id << "\">"
							"No, go back</a>\n"
					"</div>\n"
				"</form>\n"
			"</div>\n";

		return;
	}

	// Edit profile
	if (sim_.req_->method == server::HttpRequest::POST && view_type == FULL) {
		string new_username;
		// Validate all fields
		fv.validateNotBlank(new_username, "username", "Username", 30);
		fv.validateNotBlank(first_name, "first_name", "First Name");
		fv.validateNotBlank(last_name, "last_name", "Last Name", 60);
		fv.validateNotBlank(email, "email", "Email", 60);

		// If all fields are ok
		if (fv.noErrors())
			try {
				pstmt.reset(sim_.db_conn()->prepareStatement("UPDATE IGNORE users "
					"SET username=?, first_name=?, last_name=?, email=? "
					"WHERE id=?"));
				pstmt->setString(1, new_username);
				pstmt->setString(2, first_name);
				pstmt->setString(3, last_name);
				pstmt->setString(4, email);
				pstmt->setString(5, user_id);

				if (pstmt->executeUpdate() == 1) {
					fv.addError("Update successful");
					username = new_username;

				} else if (username != new_username)
					fv.addError("Username '" + new_username + "' is taken");

			} catch (const std::exception& e) {
				E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__,
					__LINE__, e.what());

			} catch (...) {
				E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
			}
	}

	Template templ(sim_, "");
	templ << fv.errors() << "<div class=\"form-container\">\n"
		"<h1>Edit account</h1>\n"
		"<form method=\"post\">\n"
			// Username
			"<div class=\"field-group\">\n"
				"<label>Username</label>\n"
				"<input type=\"text\" name=\"username\" value=\""
					<< htmlSpecialChars(username) << "\" size=\"24\" "
					"maxlength=\"30\" " << (view_type == READ_ONLY ? "readonly"
						: "required") <<  ">\n"
			"</div>\n"
			// First Name
			"<div class=\"field-group\">\n"
				"<label>First name</label>\n"
				"<input type=\"text\" name=\"first_name\" value=\""
					<< htmlSpecialChars(first_name) << "\" size=\"24\""
					"maxlength=\"60\" " << (view_type == READ_ONLY ? "readonly"
						: "required") <<  ">\n"
			"</div>\n"
			// Last name
			"<div class=\"field-group\">\n"
				"<label>Last name</label>\n"
				"<input type=\"text\" name=\"last_name\" value=\""
					<< htmlSpecialChars(last_name) << "\" size=\"24\""
					"maxlength=\"60\" " << (view_type == READ_ONLY ? "readonly"
						: "required") <<  ">\n"
			"</div>\n"
			// Email
			"<div class=\"field-group\">\n"
				"<label>Email</label>\n"
				"<input type=\"email\" name=\"email\" value=\""
					<< htmlSpecialChars(email) << "\" size=\"24\""
					"maxlength=\"60\" " << (view_type == READ_ONLY ? "readonly"
						: "required") <<  ">\n"
			"</div>\n";

	// Buttons
	if (view_type == FULL)
		templ << "<div>\n"
				"<input class=\"btn\" type=\"submit\" value=\"Update\">\n"
				"<a class=\"btn-danger\" style=\"float:right\" href=\"/u/"
					<< user_id << "/delete\">Delete account</a>\n"
			"</div>\n";

	templ << "</form>\n"
		"</div>\n";
}
