#include "form_validator.h"
#include "sim_session.h"
#include "sim_template.h"
#include "sim_user.h"

#include <cppconn/prepared_statement.h>
#include <simlib/logger.h>
#include <simlib/random.h>
#include <simlib/sha.h>

using std::map;
using std::string;
using std::unique_ptr;

struct Sim::User::Data {
	string user_id, username, first_name, last_name, email;
	uint user_type, permissions;
};

class Sim::User::TemplateWithMenu : public Template {
public:
	TemplateWithMenu(Sim& sim, const string& user_id, const std::string& title,
		const std::string& styles = "", const std::string& scripts = "");

	void printUser(const Data& data);
};

constexpr uint PERM_VIEW = 1;
constexpr uint PERM_EDIT = 2;
constexpr uint PERM_CHANGE_PASS = 4;
constexpr uint PERM_ADMIN_CHANGE_PASS = 8;
constexpr uint PERM_DELETE = 16;
constexpr uint PERM_ADMIN = 31;

constexpr uint PERM_MAKE_ADMIN = 32;
constexpr uint PERM_MAKE_TEACHER = 64;
constexpr uint PERM_DEMOTE = 128;

/// @brief Returns a set of operation the viewer is allowed to do over the user
static uint permissions(const string& viewer_id, uint viewer_type,
	const string& user_id, uint user_type)
{
	uint viewer = viewer_type + (viewer_id != "1");
	if (viewer_id == user_id) {
		constexpr uint perm[4] = {
			// SIM root
			PERM_VIEW | PERM_EDIT | PERM_CHANGE_PASS,
			// Admin
			PERM_VIEW | PERM_EDIT | PERM_CHANGE_PASS | PERM_DEMOTE |
				PERM_DELETE,
			// Teacher
			PERM_VIEW | PERM_EDIT | PERM_CHANGE_PASS | PERM_DEMOTE |
				PERM_DELETE,
			// Normal
			PERM_VIEW | PERM_EDIT | PERM_CHANGE_PASS | PERM_DELETE,
		};
		return perm[viewer];
	}

	uint user = user_type + (user_id != "1");
	// Permission table [ viewer ][ user ]
	constexpr uint perm[4][4] = {
		{ // SIM root
			PERM_VIEW | PERM_EDIT | PERM_CHANGE_PASS, // SIM root
			PERM_ADMIN | PERM_DEMOTE, // Admin
			PERM_ADMIN | PERM_MAKE_ADMIN | PERM_DEMOTE, // Teacher
			PERM_ADMIN | PERM_MAKE_ADMIN | PERM_MAKE_TEACHER // Normal
		}, { // Admin
			PERM_VIEW, // SIM root
			PERM_VIEW, // Admin
			PERM_ADMIN | PERM_DEMOTE, // Teacher
			PERM_ADMIN | PERM_MAKE_TEACHER // Normal
		}, { // Teacher
			0, // SIM root
			PERM_VIEW, // Admin
			PERM_VIEW, // Teacher
			PERM_VIEW // Normal
		}, { // Normal
			0, // SIM root
			0, // Admin
			0, // Teacher
			0 // Normal
		}
	};
	return perm[viewer][user];
}

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
	// TODO: URI parser
	int res_code = strToNum(data.user_id, sim_.req_->target, arg_beg, '/');
	if (res_code <= 0)
		return listUsers();

	arg_beg += res_code + 1;

	// Get user information
	unique_ptr<sql::PreparedStatement> pstmt(sim_.db_conn->prepareStatement(
		"SELECT username, first_name, last_name, email, type "
		"FROM users WHERE id=?"));
	pstmt->setString(1, data.user_id);

	unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
	if (res->next()) {
		data.username = res->getString(1);
		data.first_name = res->getString(2);
		data.last_name = res->getString(3);
		data.email = res->getString(4);
		data.user_type = res->getUInt(5);
	} else
		return sim_.error404();

	// TODO: write test suite!
	// Check permissions
	data.permissions = permissions(sim_.session->user_id,
		sim_.session->user_type, data.user_id, data.user_type);

	if (~data.permissions & PERM_VIEW)
		return sim_.error403();

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
				unique_ptr<sql::PreparedStatement> pstmt(sim_.db_conn->
					prepareStatement("SELECT id, salt, password FROM `users` "
						"WHERE username=?"));
				pstmt->setString(1, username);

				unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
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
				fv.addError("Internal server error");
				errlog("Caught exception: ", __FILE__, ':', toString(__LINE__),
					" -> ", e.what());
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

				unique_ptr<sql::PreparedStatement> pstmt(sim_.db_conn->
					prepareStatement("INSERT IGNORE `users` (username, "
							"first_name, last_name, email, salt, password) "
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

					unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
					if (res->next())
						sim_.session->create(res->getString(1));

					return sim_.redirect("/");
				}

				fv.addError("Username taken");

			} catch (const std::exception& e) {
				fv.addError("Internal server error");
				errlog("Caught exception: ", __FILE__, ':', toString(__LINE__),
					" -> ", e.what());
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

void Sim::User::listUsers() {
	if (sim_.session->user_type > 1)
		return sim_.error403();

	Template templ(sim_, "Users list", ".body{margin-left:30px}");
	templ << "<h1>Users</h1>";
	try {
		unique_ptr<sql::PreparedStatement> pstmt(sim_.db_conn->prepareStatement(
			"SELECT id, username, first_name, last_name, email, type "
			"FROM users ORDER BY id"));
		unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

		templ << "<table class=\"users\">\n"
			"<thead>\n"
				"<tr>"
					"<th class=\"uid\">Uid</th>"
					"<th class=\"username\">Username</th>"
					"<th class=\"first-name\">First name</th>"
					"<th class=\"last-name\">Last name</th>"
					"<th class=\"email\">Email</th>"
					"<th class=\"type\">Type</th>"
					"<th class=\"actions\">Actions</th>"
				"</tr>\n"
			"</thead>\n"
			"<tbody>\n";
		while (res->next()) {
			string uid = res->getString(1);
			uint utype = res->getUInt(6);
			uint perm = permissions(sim_.session->user_id,
				sim_.session->user_type, res->getString(1), utype);
			if (~perm & PERM_VIEW)
				continue;

			templ << "<tr>"
				"<td>" << uid << "</td>"
				"<td>" << res->getString(2) << "</td>"
				"<td>" << res->getString(3) << "</td>"
				"<td>" << res->getString(4) << "</td>"
				"<td>" << res->getString(5) << "</td>"
				"<td>" << (utype >= 2 ? "Normal"
					: (utype == 1 ? "Teacher" : "Admin")) << "</td>"
				"<td>"
					"<a class=\"btn-small\" href=\"/u/"
						<< uid << "\">View profile</a>";

			if (perm & PERM_CHANGE_PASS)
				templ <<  "<a class=\"btn-small blue\" href=\"/u/" << uid
					<< "/change-password\">Change password</a>";
			if (perm & PERM_DELETE)
				templ <<  "<a class=\"btn-small red\" href=\"/u/" << uid
					<< "/delete\">Delete account</a>";

			templ << "</td>"
				"</tr>\n";
		}

		templ << "</tbody>\n"
			"</table>\n";

	} catch (const std::exception& e) {
		errlog("Caught exception: ", __FILE__, ':', toString(__LINE__), " -> ",
			e.what());
		return sim_.error500();
	}
}

void Sim::User::editProfile(Data& data) {
	FormValidator fv(sim_.req_->form_data);
	if (sim_.req_->method == server::HttpRequest::POST &&
			data.permissions & PERM_EDIT) {
		string new_username, user_type;
		// Validate all fields
		fv.validateNotBlank(new_username, "username", "Username", 30);
		fv.validateNotBlank(user_type, "type", "Account type");
		fv.validateNotBlank(data.first_name, "first_name", "First Name");
		fv.validateNotBlank(data.last_name, "last_name", "Last Name", 60);
		fv.validateNotBlank(data.email, "email", "Email", 60);

		uint new_user_type = (user_type == "0" ? 0 :
			(user_type == "1" ? 1 : 2));

		// Check if change of user type is allowed
		constexpr uint needed_promote_privilege[3] = {
			PERM_MAKE_ADMIN,
			PERM_MAKE_TEACHER,
			0 // Just in case
		};
		// Demote
		if (new_user_type > data.user_type && ~data.permissions & PERM_DEMOTE) {
			stdlog("golka rolka");
			new_user_type = data.user_type;
		}
		// Promote
		else if (new_user_type < data.user_type &&
			~data.permissions & needed_promote_privilege[new_user_type])
		{
			stdlog("golka rolka");
			new_user_type = data.user_type;
		}

		// If all fields are ok
		if (fv.noErrors())
			try {
				unique_ptr<sql::PreparedStatement> pstmt(sim_.db_conn->
					prepareStatement("UPDATE IGNORE users "
					"SET username=?, first_name=?, last_name=?, email=?, "
					"type=? WHERE id=?"));
				pstmt->setString(1, new_username);
				pstmt->setString(2, data.first_name);
				pstmt->setString(3, data.last_name);
				pstmt->setString(4, data.email);
				pstmt->setUInt(5, new_user_type);
				pstmt->setString(6, data.user_id);

				if (pstmt->executeUpdate() == 1) {
					fv.addError("Update successful");
					// Update user info
					data.username = new_username;
					data.user_type = new_user_type;
					data.permissions = permissions(sim_.session->user_id,
						sim_.session->user_type, data.user_id, data.user_type);

					if (data.user_id == sim_.session->user_id) {
						sim_.session->user_type = data.user_type;
						sim_.session->username = new_username;
					}

				} else if (data.username != new_username)
					fv.addError(concat("Username '", new_username,
						"' is taken"));

			} catch (const std::exception& e) {
				fv.addError("Internal server error");
				errlog("Caught exception: ", __FILE__, ':', toString(__LINE__),
					" -> ", e.what());
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
					"maxlength=\"30\" " << (data.permissions & PERM_EDIT ?
						"required": "readonly") <<  ">\n"
			"</div>\n"
			// Account type
			"<div class=\"field-group\">\n"
				"<label>Account type</label>\n"
				"<select name=\"type\""
				<< (data.permissions & (PERM_MAKE_ADMIN | PERM_MAKE_TEACHER |
					PERM_DEMOTE) ? "" : " disabled") << ">";

	switch (data.user_type) {
		case 0: // Admin
			templ << "<option value=\"0\" selected>Admin</option>";
			if (data.permissions & PERM_DEMOTE)
				templ << "<option value=\"1\">Teacher</option>"
					     "<option value=\"2\">Normal</option>";
			break;

		case 1: // Teacher
			if (data.permissions & PERM_MAKE_ADMIN)
				templ << "<option value=\"0\">Admin</option>";
			templ << "<option value=\"1\" selected>Teacher</option>";
			if (data.permissions & PERM_DEMOTE)
				templ << "<option value=\"2\">Normal</option>";
			break;

		default: // Normal
			if (data.permissions & PERM_MAKE_ADMIN)
				templ << "<option value=\"0\">Admin</option>";
			if (data.permissions & PERM_MAKE_TEACHER)
				templ << "<option value=\"1\">Teacher</option>";
			templ << "<option value=\"2\" selected>Normal</option>";
	}
	templ << "</select>\n";

	if ((data.permissions & (PERM_MAKE_ADMIN | PERM_MAKE_TEACHER | PERM_DEMOTE))
			== 0)
	{
		templ << "<input type=\"hidden\" name=\"type\" value=\""
			<< toString(data.user_type) << "\">\n";
	}

	templ << "</div>\n"

			// First Name
			"<div class=\"field-group\">\n"
				"<label>First name</label>\n"
				"<input type=\"text\" name=\"first_name\" value=\""
					<< htmlSpecialChars(data.first_name) << "\" size=\"24\""
					"maxlength=\"60\" " << (data.permissions & PERM_EDIT ?
						"required" : "readonly") <<  ">\n"
			"</div>\n"
			// Last name
			"<div class=\"field-group\">\n"
				"<label>Last name</label>\n"
				"<input type=\"text\" name=\"last_name\" value=\""
					<< htmlSpecialChars(data.last_name) << "\" size=\"24\""
					"maxlength=\"60\" " << (data.permissions & PERM_EDIT ?
						"required" : "readonly") <<  ">\n"
			"</div>\n"
			// Email
			"<div class=\"field-group\">\n"
				"<label>Email</label>\n"
				"<input type=\"email\" name=\"email\" value=\""
					<< htmlSpecialChars(data.email) << "\" size=\"24\""
					"maxlength=\"60\" " << (data.permissions & PERM_EDIT ?
						"required" : "readonly") <<  ">\n"
			"</div>\n";

	// Buttons
	if (data.permissions & (PERM_EDIT | PERM_DELETE)) {
		templ << "<div>\n";
		if (data.permissions & PERM_EDIT)
			templ << "<input class=\"btn\" type=\"submit\" value=\"Update\">\n";
		if (data.permissions & PERM_DELETE)
			templ << "<a class=\"btn red\" style=\"float:right\" href=\"/u/"
				<< data.user_id << "/delete\">Delete account</a>\n";
		templ << "</div>\n";
	}

	templ << "</form>\n"
		"</div>\n";
}

void Sim::User::changePassword(Data& data) {
	if (~data.permissions & PERM_CHANGE_PASS)
		return sim_.error403();

	FormValidator fv(sim_.req_->form_data);
	if (sim_.req_->method == server::HttpRequest::POST) {
		// Validate all fields
		string old_password, password1, password2;
		if (~data.permissions & PERM_ADMIN_CHANGE_PASS)
			fv.validate(old_password, "old_password", "Old password");

		if (fv.validate(password1, "password1", "New password") &&
				fv.validate(password2, "password2", "New password (repeat)") &&
				password1 != password2)
			fv.addError("Passwords do not match");

		// If all fields are ok
		if (fv.noErrors())
			try {
				unique_ptr<sql::PreparedStatement> pstmt(sim_.db_conn->
					prepareStatement("SELECT salt, password FROM users "
						"WHERE id=?"));
				pstmt->setString(1, data.user_id);

				unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
				if (!res->next()) {
					fv.addError("Cannot get user password");

				} else if (~data.permissions & PERM_ADMIN_CHANGE_PASS &&
					!slowEqual(sha3_512(res->getString(1) + old_password),
						res->getString(2)))
				{
					fv.addError("Wrong old password");

				} else {
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
				fv.addError("Internal server error");
				errlog("Caught exception: ", __FILE__, ':', toString(__LINE__),
					" -> ", e.what());
			}
	}

	TemplateWithMenu templ(sim_, data.user_id, "Change password");
	templ.printUser(data);
	templ << fv.errors() << "<div class=\"form-container\">\n"
			"<h1>Change password</h1>\n"
			"<form method=\"post\">\n";

	// Old password
	if (~data.permissions & PERM_ADMIN_CHANGE_PASS)
		templ << "<div class=\"field-group\">\n"
					"<label>Old password</label>\n"
					"<input type=\"password\" name=\"old_password\" "
						"size=\"24\">\n"
				"</div>\n";

	// New password
	templ << "<div class=\"field-group\">\n"
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

void Sim::User::deleteAccount(Data& data) {
	if (~data.permissions & PERM_DELETE)
		return sim_.error403();

	FormValidator fv(sim_.req_->form_data);
	if (sim_.req_->method == server::HttpRequest::POST && fv.exist("delete"))
		try {
			// Change contests and problems owner id to 1
			unique_ptr<sql::PreparedStatement> pstmt(sim_.db_conn->
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
			fv.addError("Internal server error");
			errlog("Caught exception: ", __FILE__, ':', toString(__LINE__),
				" -> ", e.what());
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
					"<button class=\"btn red\" type=\"submit\" "
						"name=\"delete\">Yes, I'm sure</button>\n"
					"<a class=\"btn\" href=\""
							<< sim_.req_->headers.get("Referer") << "\">"
						"No, go back</a>\n"
				"</div>\n"
			"</form>\n"
		"</div>\n";
}
