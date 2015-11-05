#include "form_validator.h"
#include "sim_session.h"
#include "sim_template.h"
#include "sim_user.h"

#include "../simlib/include/logger.h"
#include "../simlib/include/random.h"
#include "../simlib/include/sha.h"

#include <cppconn/prepared_statement.h>

using std::string;
using std::map;

struct Sim::User::Data {
	string user_id, username, first_name, last_name, email;
	unsigned user_type;
	enum ViewType { FULL, READ_ONLY } view_type;
};

class Sim::User::TemplateWithMenu : public Template {
public:
	TemplateWithMenu(Sim& sim, const string& user_id, const std::string& title,
		const std::string& styles = "", const std::string& scripts = "");

	void printUser(const Data& data);
};

Sim::User::TemplateWithMenu::TemplateWithMenu(Sim& sim, const string& user_id,
	const string& title, const string& styles, const string& scripts)
		: Sim::Template(sim, title, ".body{margin-left:190px}" + styles,
			scripts) {
	if (sim.session->open() != Session::OK)
		return;

	*this << "<ul class=\"menu\">\n"
			"<span>YOUR ACCOUNT</span>"
			"<a href=\"/u/" << sim.session->user_id << "\">Edit profile</a>\n"
			"<a href=\"/u/" << sim.session->user_id << "/change-password\">"
				"Change password</a>\n";

	if (sim.session->user_id != user_id)
		*this << "<span>VIEWED ACCOUNT</span>"
			"<a href=\"/u/" << user_id << "\">Edit profile</a>\n"
			"<a href=\"/u/" << user_id << "/change-password\">Change password"
				"</a>\n";

	*this << "</ul>";
}

void Sim::User::TemplateWithMenu::printUser(const Data& data) {
	*this << "<h4><a href=\"/u/" << data.user_id << "\">" << data.username
		<< "</a>" << " (" << data.first_name << " " << data.last_name
		<< ")</h4>\n";
}

void Sim::User::handle() {
	if (sim_.session->open() != Session::OK)
		return sim_.redirect("/login" + sim_.req_->target);

	size_t arg_beg = 3;
	Data data;

	// Extract user id
	int res_code = strToNum(data.user_id, sim_.req_->target, arg_beg, '/');
	if (res_code == -1)
		return sim_.error404();

	arg_beg += res_code + 1;

	// Get user information
	UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->prepareStatement(
		"SELECT username, first_name, last_name, email, type "
		"FROM users WHERE id=?"));
	pstmt->setString(1, data.user_id);

	UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
	if (res->next()) {
		data.username = res->getString(1);
		data.first_name = res->getString(2);
		data.last_name = res->getString(3);
		data.email = res->getString(4);
		data.user_type = res->getUInt(5);
	} else
		return sim_.error404();

	data.view_type = Data::FULL;
	/* Check permissions
	* +---------------+---------+-------+---------+--------+
	* | user \ viewer | id == 1 | admin | teacher | normal |
	* +---------------+---------+-------+---------+--------+
	* |    normal     |  FULL   | FULL  |   RDO   |  ---   |
	* |    teacher    |  FULL   | FULL  |   ---   |  ---   |
	* |     admin     |  FULL   |  RDO  |   ---   |  ---   |
	* +---------------+---------+-------+---------+--------+
	*/
	if (data.user_id == sim_.session->user_id) {}

	else if (sim_.session->user_type >= 2 ||
			(sim_.session->user_type == 1 && data.user_type < 2))
		return sim_.error403();

	else if (sim_.session->user_type == 1 ||
				(sim_.session->user_type == 0 && sim_.session->user_id != "1"))
		data.view_type = Data::READ_ONLY;

	// Change password
	if (compareTo(sim_.req_->target, arg_beg, '/', "change-password") == 0)
		return changePassword(data);

	// Delete account
	if (compareTo(sim_.req_->target, arg_beg, '/', "delete") == 0)
		return deleteAccount(data);

	// Edit account
	editProfile(data);
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
				UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
					prepareStatement("SELECT id, salt, password FROM `users` "
						"WHERE username=?"));
				pstmt->setString(1, username);

				UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
				if (res->next()) {
					if (!slowEqual(sha3_512(res->getString(2) + password),
							res->getString(3)))
						goto label_invalid;

					// Delete old session
					if (sim_.session->open() == Session::OK)
						sim_.session->destroy();
					// Create new
					sim_.session->create(res->getString(1));

					// If there is redirection string, redirect
					if (sim_.req_->target.size() > 6)
						return sim_.redirect(sim_.req_->target.substr(6));

					return sim_.redirect("/");

				} else {
				label_invalid:
					fv.addError("Invalid username or password");
				}

			} catch (const std::exception& e) {
				error_log("Caught exception: ", __FILE__, ':',
					toString(__LINE__), " - ", e.what());
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
						<< htmlSpecialChars(username) << "\" size=\"24\" "
						"maxlength=\"30\" required>\n"
				"</div>\n"
				// Password
				"<div class=\"field-group\">\n"
					"<label>Password</label>\n"
					"<input type=\"password\" name=\"password\" "
						"size=\"24\">\n"
				"</div>\n"
				"<input class=\"btn\" type=\"submit\" value=\"Log in\">\n"
			"</form>\n"
		"</div>\n";
}

void Sim::User::logout() {
	if (sim_.session->open() == Session::OK)
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
			fv.addError("Passwords do not match");

		// If all fields are ok
		if (fv.noErrors())
			try {
				char salt_bin[32];
				fillRandomly(salt_bin, 32);
				string salt = toHex(salt_bin, 32);

				UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
					prepareStatement("INSERT IGNORE INTO `users` "
						"(username, first_name, last_name, email, salt, "
							"password) "
						"VALUES(?, ?, ?, ?, ?, ?)"));
				pstmt->setString(1, username);
				pstmt->setString(2, first_name);
				pstmt->setString(3, last_name);
				pstmt->setString(4, email);
				pstmt->setString(5, salt);
				pstmt->setString(6, sha3_512(salt + password1));

				if (pstmt->executeUpdate() == 1) {
					pstmt.reset(sim_.db_conn->prepareStatement(
						"SELECT id FROM `users` WHERE username=?"));
					pstmt->setString(1, username);

					UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
					if (res->next())
						sim_.session->create(res->getString(1));

					return sim_.redirect("/");
				}

				fv.addError("Username taken");

			} catch (const std::exception& e) {
				error_log("Caught exception: ", __FILE__, ':',
					toString(__LINE__), " - ", e.what());
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
						<< htmlSpecialChars(username) << "\" size=\"24\" "
						"maxlength=\"30\" required>\n"
				"</div>\n"
				// First Name
				"<div class=\"field-group\">\n"
					"<label>First name</label>\n"
					"<input type=\"text\" name=\"first_name\" value=\""
						<< htmlSpecialChars(first_name) << "\" size=\"24\" "
						"maxlength=\"60\" required>\n"
				"</div>\n"
				// Last name
				"<div class=\"field-group\">\n"
					"<label>Last name</label>\n"
					"<input type=\"text\" name=\"last_name\" value=\""
						<< htmlSpecialChars(last_name) << "\" size=\"24\" "
						"maxlength=\"60\" required>\n"
				"</div>\n"
				// Email
				"<div class=\"field-group\">\n"
					"<label>Email</label>\n"
					"<input type=\"email\" name=\"email\" value=\""
						<< htmlSpecialChars(email) << "\" size=\"24\" "
						"maxlength=\"60\" required>\n"
				"</div>\n"
				// Password
				"<div class=\"field-group\">\n"
					"<label>Password</label>\n"
					"<input type=\"password\" name=\"password1\" size=\"24\">\n"
				"</div>\n"
				// Password (repeat)
				"<div class=\"field-group\">\n"
					"<label>Password (repeat)</label>\n"
					"<input type=\"password\" name=\"password2\" size=\"24\">\n"
				"</div>\n"
				"<input class=\"btn\" type=\"submit\" value=\"Sign up\">\n"
			"</form>\n"
		"</div>\n";
}

void Sim::User::changePassword(Data& data) {
	if (data.view_type == Data::READ_ONLY)
		return sim_.error403();

	// TODO: allow admins to change other's passwords
	FormValidator fv(sim_.req_->form_data);
	if (sim_.req_->method == server::HttpRequest::POST) {
		// Validate all fields
		string old_password, password1, password2;
		fv.validate(old_password, "old_password", "Old password");
		if (fv.validate(password1, "password1", "New password") &&
				fv.validate(password2, "password2", "New password (repeat)") &&
				password1 != password2)
			fv.addError("Passwords do not match");

		// If all fields are ok
		if (fv.noErrors())
			try {
				UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
					prepareStatement("SELECT salt, password FROM users "
						"WHERE id=?"));
				pstmt->setString(1, data.user_id);

				UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
				if (!res->next())
					fv.addError("Cannot get user password");

				else if (!slowEqual(sha3_512(res->getString(1) + old_password),
						res->getString(2)))
					fv.addError("Wrong password");

				else {
					char salt_bin[32];
					fillRandomly(salt_bin, 32);
					string salt = toHex(salt_bin, 32);

					pstmt.reset(sim_.db_conn->prepareStatement(
						"UPDATE users SET salt=?, password=? WHERE id=?"));
					pstmt->setString(1, salt);
					pstmt->setString(2, sha3_512(salt + password1));
					pstmt->setString(3, data.user_id);

					if (pstmt->executeUpdate() == 1)
						fv.addError("Password changed");
				}

			} catch (const std::exception& e) {
				error_log("Caught exception: ", __FILE__, ':',
					toString(__LINE__), " - ", e.what());
			}
	}

	TemplateWithMenu templ(sim_, data.user_id, "Change password");
	templ.printUser(data);
	templ << fv.errors() << "<div class=\"form-container\">\n"
			"<h1>Change password</h1>\n"
			"<form method=\"post\">\n"
				// Old password
				"<div class=\"field-group\">\n"
					"<label>Old password</label>\n"
					"<input type=\"password\" name=\"old_password\" "
						"size=\"24\">\n"
				"</div>\n"
				// New password
				"<div class=\"field-group\">\n"
					"<label>New password</label>\n"
					"<input type=\"password\" name=\"password1\" size=\"24\">\n"
				"</div>\n"
				// New password (repeat)
				"<div class=\"field-group\">\n"
					"<label>New password (repeat)</label>\n"
					"<input type=\"password\" name=\"password2\" size=\"24\">\n"
				"</div>\n"
				"<input class=\"btn\" type=\"submit\" value=\"Update\">\n"
			"</form>\n"
		"</div>\n";
}

void Sim::User::editProfile(Data& data) {
	/* The ability to change user type
	* +---------------+---------+-------+-------+
	* | user \ viewer | id == 1 | admin | other |
	* +---------------+---------+-------+-------+
	* |    id == 1    |   NO    |  NO   |  NO   |
	* |    admin      |   YES   |  NO   |  NO   |
	* |    other      |   YES   |  YES  |  NO   |
	* +---------------+---------+-------+-------+
	*/
	bool can_change_user_type = data.user_id != "1" &&
		sim_.session->user_type == 0 && (data.user_type > 0 ||
			sim_.session->user_id == "1");

	FormValidator fv(sim_.req_->form_data);
	if (sim_.req_->method == server::HttpRequest::POST &&
			data.view_type == Data::FULL) {
		string new_username, user_type;
		// Validate all fields
		fv.validateNotBlank(new_username, "username", "Username", 30);
		fv.validateNotBlank(user_type, "type", "Account type");
		fv.validateNotBlank(data.first_name, "first_name", "First Name");
		fv.validateNotBlank(data.last_name, "last_name", "Last Name", 60);
		fv.validateNotBlank(data.email, "email", "Email", 60);

		// If all fields are ok
		if (fv.noErrors())
			try {
				UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
					prepareStatement("UPDATE IGNORE users "
					"SET username=?, first_name=?, last_name=?, email=?, "
					"type=? WHERE id=?"));
				pstmt->setString(1, new_username);
				pstmt->setString(2, data.first_name);
				pstmt->setString(3, data.last_name);
				pstmt->setString(4, data.email);
				pstmt->setUInt(5, can_change_user_type ? (user_type == "0" ? 0
						: user_type == "1" ? 1 : 2) : data.user_type);
				pstmt->setString(6, data.user_id);

				if (pstmt->executeUpdate() == 1) {
					fv.addError("Update successful");
					// Update user info
					data.username = new_username;
					data.user_type = (user_type == "0" ? 0
						: user_type == "1" ? 1 : 2);

					if (data.user_id == sim_.session->user_id)
						// We do not have to actualise user_type because nobody
						// can change their account type
						sim_.session->username = new_username;

				} else if (data.username != new_username)
					fv.addError("Username '" + new_username + "' is taken");

			} catch (const std::exception& e) {
				error_log("Caught exception: ", __FILE__, ':',
					toString(__LINE__), " - ", e.what());
			}
	}

	TemplateWithMenu templ(sim_, data.user_id, "Edit profile");
	templ.printUser(data);
	templ << fv.errors() << "<div class=\"form-container\">\n"
		"<h1>Edit account</h1>\n"
		"<form method=\"post\">\n"
			// Username
			"<div class=\"field-group\">\n"
				"<label>Username</label>\n"
				"<input type=\"text\" name=\"username\" value=\""
					<< htmlSpecialChars(data.username) << "\" size=\"24\" "
					"maxlength=\"30\" " << (data.view_type == Data::READ_ONLY ?
						"readonly" : "required") <<  ">\n"
			"</div>\n"
			// Account type
			"<div class=\"field-group\">\n"
				"<label>Account type</label>\n"
				"<select name=\"type\"" << (can_change_user_type ? ""
					: " disabled") << ">"
					"<option value=\"0\"" << (data.user_type == 0 ? " selected"
						: "") << ">Admin</option>"
					"<option value=\"1\"" << (data.user_type == 1 ? " selected"
						: "") << ">Teacher</option>"
					"<option value=\"2\"" << (data.user_type > 1 ? " selected"
						: "") << ">Normal</option>"
				"</select>\n";
	if (!can_change_user_type)
		templ << "<input type=\"hidden\" name=\"type\" value=\""
			<< toString<uint64_t>(data.user_type) << "\">\n";
	templ << "</div>\n"
			// First Name
			"<div class=\"field-group\">\n"
				"<label>First name</label>\n"
				"<input type=\"text\" name=\"first_name\" value=\""
					<< htmlSpecialChars(data.first_name) << "\" size=\"24\""
					"maxlength=\"60\" " << (data.view_type == Data::READ_ONLY ?
						"readonly" : "required") <<  ">\n"
			"</div>\n"
			// Last name
			"<div class=\"field-group\">\n"
				"<label>Last name</label>\n"
				"<input type=\"text\" name=\"last_name\" value=\""
					<< htmlSpecialChars(data.last_name) << "\" size=\"24\""
					"maxlength=\"60\" " << (data.view_type == Data::READ_ONLY ?
						"readonly" : "required") <<  ">\n"
			"</div>\n"
			// Email
			"<div class=\"field-group\">\n"
				"<label>Email</label>\n"
				"<input type=\"email\" name=\"email\" value=\""
					<< htmlSpecialChars(data.email) << "\" size=\"24\""
					"maxlength=\"60\" " << (data.view_type == Data::READ_ONLY ?
						"readonly" : "required") <<  ">\n"
			"</div>\n";

	// Buttons
	if (data.view_type == Data::FULL)
		templ << "<div>\n"
				"<input class=\"btn\" type=\"submit\" value=\"Update\">\n"
				"<a class=\"btn-danger\" style=\"float:right\" href=\"/u/"
					<< data.user_id << "/delete\">Delete account</a>\n"
			"</div>\n";

	templ << "</form>\n"
		"</div>\n";
}

void Sim::User::deleteAccount(Data& data) {
	FormValidator fv(sim_.req_->form_data);
	// Deleting "root" account (id 1) is forbidden
	if (data.user_id == "1") {
		TemplateWithMenu templ(sim_, data.user_id, "Delete account");
		templ.printUser(data);
		templ << "<h1>You cannot delete SIM root account</h1>";
		return;
	}

	if (data.view_type == Data::READ_ONLY)
		return sim_.error403();

	if (sim_.req_->method == server::HttpRequest::POST && fv.exist("delete"))
		try {
			// Change contests and problems owner id to 1
			UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
				prepareStatement("UPDATE rounds r, problems p "
				"SET r.owner=1, p.owner=1 "
				"WHERE r.owner=? OR p.owner=?"));
			pstmt->setString(1, data.user_id);
			pstmt->setString(2, data.user_id);
			pstmt->executeUpdate();

			// Delete submissions
			pstmt.reset(sim_.db_conn->prepareStatement(
				"DELETE FROM submissions WHERE user_id=?"));
			pstmt->setString(1, data.user_id);
			pstmt->executeUpdate();

			// Delete from users_to_contests
			pstmt.reset(sim_.db_conn->prepareStatement(
				"DELETE FROM users_to_contests WHERE user_id=?"));
			pstmt->setString(1, data.user_id);
			pstmt->executeUpdate();

			// Delete user
			pstmt.reset(sim_.db_conn->prepareStatement(
				"DELETE FROM users WHERE id=?"));
			pstmt->setString(1, data.user_id);

			if (pstmt->executeUpdate() > 0) {
				if (data.user_id == sim_.session->user_id)
					sim_.session->destroy();
				return sim_.redirect("/");
			}

		} catch (const std::exception& e) {
			error_log("Caught exception: ", __FILE__, ':', toString(__LINE__),
			" - ", e.what());
		}

	TemplateWithMenu templ(sim_, data.user_id, "Delete account");
	templ.printUser(data);
	templ << fv.errors() << "<div class=\"form-container\">\n"
			"<h1>Delete account</h1>\n"
			"<form method=\"post\">\n"
				"<label class=\"field\">Are you sure to delete account "
					"<a href=\"/u/" << data.user_id << "\">"
					<< htmlSpecialChars(data.username) << "</a>, all its "
					"submissions and change owner of its contests and "
					"problems to SIM root?</label>\n"
				"<div class=\"submit-yes-no\">\n"
					"<button class=\"btn-danger\" type=\"submit\" "
						"name=\"delete\">Yes, I'm sure</button>\n"
					"<a class=\"btn\" href=\"/u/" << data.user_id << "\">"
						"No, go back</a>\n"
				"</div>\n"
			"</form>\n"
		"</div>\n";
}
