#include "form_validator.h"
#include "user.h"

#include <sim/utilities.h>
#include <simlib/debug.h>
#include <simlib/logger.h>
#include <simlib/process.h>
#include <simlib/random.h>
#include <simlib/sha.h>
#include <simlib/time.h>

using std::map;
using std::string;

uint User::getPermissions(const string& viewer_id, uint viewer_type,
	const string& uid, uint utype)
{
	uint viewer = viewer_type + (viewer_id != SIM_ROOT_UID);
	if (viewer_id == uid) {
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

	uint user = utype + (uid != SIM_ROOT_UID);
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

void User::userTemplate(const StringView& title, const StringView& styles,
	const StringView& scripts)
{
	baseTemplate(title, concat(".body{margin-left:190px}", styles),
		scripts);
	if (!Session::isOpen())
		return;

	append("<ul class=\"menu\">\n"
			"<span>USER</span>"
			"<a href=\"/u/", user_id, "\">View profile</a>"
			"<a href=\"/u/", user_id, "/submissions\">User submissions</a>"
			"<hr/>"
			"<a href=\"/u/", user_id, "/edit\">Edit profile</a>"
			"<a href=\"/u/", user_id, "/change-password\">Change password</a>"
		"</ul>");
}

void User::handle() {
	if (!Session::open())
		return redirect("/login?" + req->target);

	if (strToNum(user_id, url_args.extractNextArg()) <= 0)
		return listUsers();

	// Get user information
	DB::Statement stmt = db_conn.prepare(
		"SELECT username, first_name, last_name, email, type "
		"FROM users WHERE id=?");
	stmt.setString(1, user_id);

	DB::Result res = stmt.executeQuery();
	if (!res.next())
		return error404();

	username = res[1];
	first_name = res[2];
	last_name = res[3];
	email = res[4];
	user_type = res.getUInt(5);

	// TODO: write test suite!
	// Check permissions
	permissions = getPermissions(user_id, user_type);

	if (~permissions & PERM_VIEW)
		return error403();

	StringView next_arg = url_args.extractNextArg();

	// Change password
	if (next_arg == "change-password")
		return changePassword();

	// Delete account
	if (next_arg == "delete")
		return deleteAccount();

	// Edit account
	if (next_arg == "edit")
		return editProfile();

	// Edit account
	if (next_arg == "submissions")
		return userSubmissions();

	userProfile();
}

void User::login() {
	FormValidator fv(req->form_data);

	if (req->method == server::HttpRequest::POST) {
		// Try to login
		string password;
		// Validate all fields
		fv.validate(username, "username", "Username", isUsername,
			"Username can only consist of characters [a-zA-Z0-9_-]",
			USERNAME_MAX_LEN);

		fv.validate(password, "password", "Password");

		if (fv.noErrors())
			try {
				DB::Statement stmt = db_conn.prepare(
					"SELECT id, salt, password FROM `users` WHERE username=?");
				stmt.setString(1, username);

				DB::Result res = stmt.executeQuery();
				while (res.next()) {
					if (!slowEqual(sha3_512(res[2] + password), res[3]))
						break;

					// Delete old session
					if (Session::open())
						Session::destroy();

					// Create new
					Session::createAndOpen(res[1]);

					// If there is redirection string, redirect to it
					string location = url_args.extractQuery().to_string();
					return redirect(location.empty() ? "/" : location);
				}

				fv.addError("Invalid username or password");

			} catch (const std::exception& e) {
				fv.addError("Internal server error");
				ERRLOG_CATCH(e);
			}

	// Clean old data
	} else
		username = "";

	baseTemplate("Login");
	append(fv.errors(), "<div class=\"form-container\">\n"
			"<h1>Log in</h1>\n"
			"<form method=\"post\">\n"
				// Username
				"<div class=\"field-group\">\n"
					"<label>Username</label>\n"
					"<input type=\"text\" name=\"username\" value=\"",
						htmlSpecialChars(username), "\" size=\"24\" "
						"maxlength=\"", toStr(USERNAME_MAX_LEN), "\" "
						"required>\n"
				"</div>\n"
				// Password
				"<div class=\"field-group\">\n"
					"<label>Password</label>\n"
					"<input type=\"password\" name=\"password\" size=\"24\">\n"
				"</div>\n"
				"<input class=\"btn blue\" type=\"submit\" value=\"Log in\">\n"
			"</form>\n"
		"</div>\n");
}

void User::logout() {
	if (Session::open())
		Session::destroy();
	redirect("/login");
}

void User::signUp() {
	if (Session::open())
		return redirect("/");

	FormValidator fv(req->form_data);
	string password1, password2;

	if (req->method == server::HttpRequest::POST) {
		// Validate all fields
		fv.validateNotBlank(username, "username", "Username", isUsername,
			"Username can only consist of characters [a-zA-Z0-9_-]",
			USERNAME_MAX_LEN);

		fv.validateNotBlank(first_name, "first_name", "First Name",
			USER_FIRST_NAME_MAX_LEN);

		fv.validateNotBlank(last_name, "last_name", "Last Name",
			USER_LAST_NAME_MAX_LEN);

		fv.validateNotBlank(email, "email", "Email", USER_EMAIL_MAX_LEN);

		if (fv.validate(password1, "password1", "Password") &&
			fv.validate(password2, "password2", "Password (repeat)") &&
			password1 != password2)
		{
			fv.addError("Passwords do not match");
		}

		// If all fields are ok
		if (fv.noErrors())
			try {
				char salt_bin[SALT_LEN >> 1];
				fillRandomly(salt_bin, sizeof(salt_bin));
				string salt = toHex(salt_bin, sizeof(salt_bin));

				DB::Statement stmt = db_conn.prepare(
					"INSERT IGNORE `users` (username, "
							"first_name, last_name, email, salt, password) "
					"VALUES(?, ?, ?, ?, ?, ?)");
				stmt.setString(1, username);
				stmt.setString(2, first_name);
				stmt.setString(3, last_name);
				stmt.setString(4, email);
				stmt.setString(5, salt);
				stmt.setString(6, sha3_512(salt + password1));

				if (stmt.executeUpdate() == 1) {
					stmt = db_conn.prepare(
						"SELECT id FROM `users` WHERE username=?");
					stmt.setString(1, username);

					DB::Result res = stmt.executeQuery();
					if (res.next())
						Session::createAndOpen(res[1]);

					return redirect("/");
				}

				fv.addError("Username taken");

			} catch (const std::exception& e) {
				fv.addError("Internal server error");
				ERRLOG_CATCH(e);
			}

	// Clean old data
	} else {
		username = "";
		first_name = "";
		last_name = "";
		email = "";
	}

	baseTemplate("Register");
	append(fv.errors(), "<div class=\"form-container\">\n"
			"<h1>Register</h1>\n"
			"<form method=\"post\">\n"
				// Username
				"<div class=\"field-group\">\n"
					"<label>Username</label>\n"
					"<input type=\"text\" name=\"username\" value=\"",
						htmlSpecialChars(username), "\" size=\"24\" "
						"maxlength=\"", toStr(USERNAME_MAX_LEN), "\" "
						"required>\n"
				"</div>\n"
				// First Name
				"<div class=\"field-group\">\n"
					"<label>First name</label>\n"
					"<input type=\"text\" name=\"first_name\" value=\"",
						htmlSpecialChars(first_name), "\" size=\"24\" "
						"maxlength=\"", toStr(USER_FIRST_NAME_MAX_LEN), "\" "
						"required>\n"
				"</div>\n"
				// Last name
				"<div class=\"field-group\">\n"
					"<label>Last name</label>\n"
					"<input type=\"text\" name=\"last_name\" value=\"",
						htmlSpecialChars(last_name), "\" size=\"24\" "
						"maxlength=\"", toStr(USER_LAST_NAME_MAX_LEN), "\" "
						"required>\n"
				"</div>\n"
				// Email
				"<div class=\"field-group\">\n"
					"<label>Email</label>\n"
					"<input type=\"email\" name=\"email\" value=\"",
						htmlSpecialChars(email), "\" size=\"24\" "
						"maxlength=\"", toStr(USER_EMAIL_MAX_LEN), "\" "
						"required>\n"
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
				"<input class=\"btn blue\" type=\"submit\" value=\"Sign up\">\n"
			"</form>\n"
		"</div>\n");
}

void User::listUsers() {
	if (Session::user_type > UTYPE_TEACHER)
		return error403();

	baseTemplate("Users list", ".body{margin-left:30px}");
	append("<h1>Users</h1>");
	try {
		DB::Statement stmt = db_conn.prepare(
			"SELECT id, username, first_name, last_name, email, type "
			"FROM users ORDER BY id");
		DB::Result res = stmt.executeQuery();

		append("<table class=\"users\">\n"
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
			"<tbody>\n");

		while (res.next()) {
			string uid = res[1];
			uint utype = res.getUInt(6);
			uint perm = getPermissions(res[1], utype);
			if (~perm & PERM_VIEW)
				continue;

			append("<tr>"
				"<td>", uid, "</td>"
				"<td>", htmlSpecialChars(res[2]), "</td>"
				"<td>", htmlSpecialChars(res[3]), "</td>"
				"<td>", htmlSpecialChars(res[4]), "</td>"
				"<td>", htmlSpecialChars(res[5]), "</td>");

			switch (utype) {
			case UTYPE_ADMIN:
				append("<td class=\"admin\">Admin</td>");
				break;
			case UTYPE_TEACHER:
				append("<td class=\"teacher\">Teacher</td>");
				break;
			default:
				append("<td class=\"normal\">Normal</td>");
			}

			append("<td>"
					"<a class=\"btn-small\" href=\"/u/", uid, "\">"
						"View profile</a>");

			if (perm & PERM_CHANGE_PASS)
				append("<a class=\"btn-small blue\" href=\"/u/", uid,
					"/change-password\">Change password</a>");
			if (perm & PERM_DELETE)
				append("<a class=\"btn-small red\" href=\"/u/", uid,
					"/delete\">Delete account</a>");

			append("</td>"
				"</tr>\n");
		}

		append("</tbody>\n"
			"</table>\n");

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}
}

void User::userProfile() {
	userTemplate("User profile");
	printUser();
	append("<div class=\"user-info\">"
			"<div class=\"first-name\">"
				"<label>First name</label>",
				htmlSpecialChars(first_name),
			"</div>"
			"<div class=\"last-name\">"
				"<label>Last name</label>",
				htmlSpecialChars(last_name),
			"</div>"
			"<div class=\"username\">"
				"<label>Username</label>",
				htmlSpecialChars(username),
			"</div>"
			"<div class=\"type\">"
				"<label>Account type</label>");

	switch (user_type) {
	case UTYPE_ADMIN:
		append("<span class=\"admin\">Admin</span>");
		break;

	case UTYPE_TEACHER:
		append("<span class=\"teacher\">Teacher</span>");
		break;

	default:
		append("<span class=\"normal\">Normal</span>");
	}

	append("</div>"
			"<div class=\"email\">"
				"<label>Email</label>",
				htmlSpecialChars(email),
			"</div>"
		"</div>"
		"<h2>User submissions</h2>");

	printUserSubmissions(SUBMISSIONS_ON_USER_PROFILE_LIMIT);
}

void User::editProfile() {
	FormValidator fv(req->form_data);
	if (req->method == server::HttpRequest::POST && (permissions & PERM_EDIT)) {
		string new_username, new_utype_s;
		// Validate all fields
		fv.validateNotBlank(new_username, "username", "Username", isUsername,
			"Username can only consist of characters [a-zA-Z0-9_-]",
			USERNAME_MAX_LEN);

		fv.validateNotBlank(new_utype_s, "type", "Account type");

		fv.validateNotBlank(first_name, "first_name", "First Name",
			USER_FIRST_NAME_MAX_LEN);

		fv.validateNotBlank(last_name, "last_name", "Last Name",
			USER_LAST_NAME_MAX_LEN);

		fv.validateNotBlank(email, "email", "Email", USER_EMAIL_MAX_LEN);

		uint8_t new_utype = (new_utype_s == UTYPE_ADMIN_STR ? UTYPE_ADMIN :
			(new_utype_s == UTYPE_TEACHER_STR ? UTYPE_TEACHER : UTYPE_NORMAL));

		// Check if change of user type is allowed
		constexpr uint needed_promote_privilege[3] = {
			PERM_MAKE_ADMIN,
			PERM_MAKE_TEACHER,
			0 // Just in case
		};
		// Demote
		if (new_utype > user_type && (~permissions & PERM_DEMOTE))
			new_utype = user_type;

		// Promote
		else if (new_utype < user_type &&
			(~permissions & needed_promote_privilege[new_utype]))
		{
			new_utype = user_type;
		}

		// If all fields are ok
		if (fv.noErrors())
			try {
				DB::Statement stmt = db_conn.prepare("UPDATE IGNORE users "
					"SET username=?, first_name=?, last_name=?, email=?, "
					"type=? WHERE id=?");
				stmt.setString(1, new_username);
				stmt.setString(2, first_name);
				stmt.setString(3, last_name);
				stmt.setString(4, email);
				stmt.setUInt(5, new_utype);
				stmt.setString(6, user_id);

				if (stmt.executeUpdate() == 1) {
					fv.addError("Update successful");
					// Update session
					if (user_id == Session::user_id) {
						Session::user_type = new_utype;
						Session::username = new_username;
					}

					// Update user info
					user_type = new_utype;
					username = new_username;
					permissions = getPermissions(user_id, user_type);

				} else if (username != new_username)
					fv.addError(concat("Username '", new_username,
						"' is taken"));

			} catch (const std::exception& e) {
				fv.addError("Internal server error");
				ERRLOG_CATCH(e);
			}
	}

	userTemplate("Edit profile");
	printUser();
	append(fv.errors(), "<div class=\"form-container\">\n"
		"<h1>Edit account</h1>\n"
		"<form method=\"post\">\n"
			// Username
			"<div class=\"field-group\">\n"
				"<label>Username</label>\n"
				"<input type=\"text\" name=\"username\" value=\"",
					htmlSpecialChars(username), "\" size=\"24\" "
					"maxlength=\"", toStr(USERNAME_MAX_LEN), "\" ",
					(permissions & PERM_EDIT ? "required" : "readonly"), ">\n"
			"</div>\n"
			// Account type
			"<div class=\"field-group\">\n"
				"<label>Account type</label>\n"
				"<select name=\"type\"",
				(permissions & (PERM_MAKE_ADMIN | PERM_MAKE_TEACHER |
					PERM_DEMOTE) ? "" : " disabled"), '>');

	switch (user_type) {
		case UTYPE_ADMIN:
			append("<option value=\"", toStr(UTYPE_ADMIN), "\" selected>"
				"Admin</option>");
			if (permissions & PERM_DEMOTE)
				append("<option value=\"", toStr(UTYPE_TEACHER), "\">"
						"Teacher</option>"
					"<option value=\"", toStr(UTYPE_NORMAL), "\">"
						"Normal</option>");
			break;

		case UTYPE_TEACHER:
			if (permissions & PERM_MAKE_ADMIN)
				append("<option value=\"", toStr(UTYPE_ADMIN), "\">"
					"Admin</option>");
			append("<option value=\"", toStr(UTYPE_TEACHER), "\" selected>"
				"Teacher</option>");
			if (permissions & PERM_DEMOTE)
				append("<option value=\"", toStr(UTYPE_NORMAL), "\">"
					"Normal</option>");
			break;

		default: // Normal
			if (permissions & PERM_MAKE_ADMIN)
				append("<option value=\"", toStr(UTYPE_ADMIN), "\">"
					"Admin</option>");
			if (permissions & PERM_MAKE_TEACHER)
				append("<option value=\"", toStr(UTYPE_TEACHER), "\">"
					"Teacher</option>");
			append("<option value=\"", toStr(UTYPE_NORMAL), "\" selected>"
				"Normal</option>");
	}
	append("</select>\n");

	if ((permissions & (PERM_MAKE_ADMIN | PERM_MAKE_TEACHER | PERM_DEMOTE))
		== 0)
	{
		append("<input type=\"hidden\" name=\"type\" value=\"",
			toString(user_type), "\">\n");
	}

	append("</div>\n"
			// First Name
			"<div class=\"field-group\">\n"
				"<label>First name</label>\n"
				"<input type=\"text\" name=\"first_name\" value=\"",
					htmlSpecialChars(first_name), "\" size=\"24\""
					"maxlength=\"", toStr(USER_FIRST_NAME_MAX_LEN), "\" ",
					(permissions & PERM_EDIT ? "required" : "readonly"),  ">\n"
			"</div>\n"
			// Last name
			"<div class=\"field-group\">\n"
				"<label>Last name</label>\n"
				"<input type=\"text\" name=\"last_name\" value=\"",
					htmlSpecialChars(last_name), "\" size=\"24\""
					"maxlength=\"", toStr(USER_LAST_NAME_MAX_LEN), "\" ",
					(permissions & PERM_EDIT ? "required" : "readonly"),  ">\n"
			"</div>\n"
			// Email
			"<div class=\"field-group\">\n"
				"<label>Email</label>\n"
				"<input type=\"email\" name=\"email\" value=\"",
					htmlSpecialChars(email), "\" size=\"24\""
					"maxlength=\"", toStr(USER_EMAIL_MAX_LEN), "\" ",
					(permissions & PERM_EDIT ? "required" : "readonly"),  ">\n"
			"</div>\n");

	// Buttons
	if (permissions & (PERM_EDIT | PERM_DELETE)) {
		append("<div>\n");
		if (permissions & PERM_EDIT)
			append("<input class=\"btn blue\" type=\"submit\" value=\"Update\">\n");
		if (permissions & PERM_DELETE)
			append("<a class=\"btn red\" style=\"float:right\" href=\"/u/",
				user_id, "/delete?/\">Delete account</a>\n");

		append("</div>\n");
	}

	append("</form>\n"
		"</div>\n");
}

void User::changePassword() {
	if (~permissions & PERM_CHANGE_PASS)
		return error403();

	FormValidator fv(req->form_data);
	if (req->method == server::HttpRequest::POST) {
		// Validate all fields
		string old_password, password1, password2;
		if (~permissions & PERM_ADMIN_CHANGE_PASS)
			fv.validate(old_password, "old_password", "Old password");

		if (fv.validate(password1, "password1", "New password") &&
			fv.validate(password2, "password2", "New password (repeat)") &&
			password1 != password2)
		{
			fv.addError("Passwords do not match");
		}

		// If all fields are ok
		if (fv.noErrors())
			try {
				DB::Statement stmt = db_conn.prepare(
					"SELECT salt, password FROM users WHERE id=?");
				stmt.setString(1, user_id);

				DB::Result res = stmt.executeQuery();
				if (!res.next()) {
					fv.addError("Cannot get user password");

				} else if ((~permissions & PERM_ADMIN_CHANGE_PASS) &&
					!slowEqual(sha3_512(res[1] + old_password), res[2]))
				{
					fv.addError("Wrong old password");

				} else {
					char salt_bin[SALT_LEN >> 1];
					fillRandomly(salt_bin, sizeof(salt_bin));
					string salt = toHex(salt_bin, sizeof(salt_bin));

					stmt = db_conn.prepare(
						"UPDATE users SET salt=?, password=? WHERE id=?");
					stmt.setString(1, salt);
					stmt.setString(2, sha3_512(salt + password1));
					stmt.setString(3, user_id);

					if (stmt.executeUpdate() == 1)
						fv.addError("Password changed");
				}

			} catch (const std::exception& e) {
				fv.addError("Internal server error");
				ERRLOG_CATCH(e);
			}
	}

	userTemplate("Change password");
	printUser();
	append(fv.errors(), "<div class=\"form-container\">\n"
			"<h1>Change password</h1>\n"
			"<form method=\"post\">\n");

	// Old password
	if (~permissions & PERM_ADMIN_CHANGE_PASS)
		append("<div class=\"field-group\">\n"
					"<label>Old password</label>\n"
					"<input type=\"password\" name=\"old_password\" "
						"size=\"24\">\n"
				"</div>\n");

	// New password
	append("<div class=\"field-group\">\n"
					"<label>New password</label>\n"
					"<input type=\"password\" name=\"password1\" size=\"24\">\n"
				"</div>\n"
				// New password (repeat)
				"<div class=\"field-group\">\n"
					"<label>New password (repeat)</label>\n"
					"<input type=\"password\" name=\"password2\" size=\"24\">\n"
				"</div>\n"
				"<input class=\"btn blue\" type=\"submit\" value=\"Update\">\n"
			"</form>\n"
		"</div>\n");
}

void User::deleteAccount() {
	if (~permissions & PERM_DELETE)
		return error403();

	FormValidator fv(req->form_data);
	if (req->method == server::HttpRequest::POST && fv.exist("delete"))
		try {
			SignalBlocker signal_guard;
			// Change contests and problems owner id to 1
			DB::Statement stmt = db_conn.prepare("UPDATE rounds r, problems p "
				"SET r.owner=1, p.owner=1 "
				"WHERE r.owner=? OR p.owner=?");
			stmt.setString(1, user_id);
			stmt.setString(2, user_id);
			stmt.executeUpdate();

			// Delete submissions
			stmt = db_conn.prepare(
				"DELETE FROM submissions WHERE user_id=?");
			stmt.setString(1, user_id);
			stmt.executeUpdate();

			// Delete from users_to_contests
			stmt = db_conn.prepare(
				"DELETE FROM users_to_contests WHERE user_id=?");
			stmt.setString(1, user_id);
			stmt.executeUpdate();

			// Delete user
			stmt = db_conn.prepare("DELETE FROM users WHERE id=?");
			stmt.setString(1, user_id);

			if (stmt.executeUpdate()) {
				signal_guard.unblock();

				if (user_id == Session::user_id)
					Session::destroy();

				string location = url_args.extractQuery().to_string();
				return redirect(location.empty() ? "/" : location);
			}

			signal_guard.unblock();
			fv.addError("Internal server error");

		} catch (const std::exception& e) {
			fv.addError("Internal server error");
			ERRLOG_CATCH(e);
		}

	userTemplate("Delete account");
	printUser();

	// Referer or user profile
	string referer = req->headers.get("Referer");
	string prev_referer = url_args.extractQuery().to_string();
	if (prev_referer.empty()) {
		if (referer.size())
			prev_referer = referer;
		else {
			referer = '/';
			prev_referer = '/';
		}

	} else if (referer.empty())
		referer = '/';

	append(fv.errors(), "<div class=\"form-container\">\n"
			"<h1>Delete account</h1>\n"
			"<form method=\"post\" action=\"?", prev_referer ,"\">\n"
				"<label class=\"field\">Are you sure to delete account "
					"<a href=\"/u/", user_id, "\">",
					htmlSpecialChars(username), "</a>, all its "
					"submissions and change owner of its contests and "
					"problems to SIM root?</label>\n"
				"<div class=\"submit-yes-no\">\n"
					"<button class=\"btn red\" type=\"submit\" "
						"name=\"delete\">Yes, I'm sure</button>\n"
					"<a class=\"btn\" href=\"", referer, "\">No, go back</a>\n"
				"</div>\n"
			"</form>\n"
		"</div>\n");
}

void User::printUserSubmissions(uint limit) {
	try {
		string query = "SELECT s.id, s.submit_time, r3.id, r3.name, r2.id, "
				"r2.name, r.id, r.name, s.status, s.score, s.final, "
				"r2.full_results, r3.owner, u.type "
			"FROM submissions s, rounds r, rounds r2, rounds r3, users u "
			"WHERE s.user_id=? AND s.round_id=r.id AND s.parent_round_id=r2.id "
				"AND s.contest_round_id=r3.id AND r3.owner=u.id "
			"ORDER BY s.id DESC";

		if (limit > 0)
			back_insert(query, " LIMIT ", toString(limit));

		DB::Statement stmt = db_conn.prepare(query);
		stmt.setString(1, user_id);

		DB::Result res = stmt.executeQuery();
		if (res.rowCount() == 0) {
			append("<p>There are no submissions to show</p>");
			return;
		}

		append("<table class=\"submissions\">\n"
			"<thead>\n"
				"<tr>",
					"<th class=\"time\">Submission time</th>"
					"<th class=\"problem\">Problem</th>"
					"<th class=\"status\">Status</th>"
					"<th class=\"score\">Score</th>"
					"<th class=\"final\">Final</th>"
					"<th class=\"actions\">Actions</th>"
				"</tr>\n"
			"</thead>\n"
			"<tbody>\n");

		auto statusRow = [](const string& status) {
			string ret = "<td";

			if (status == "ok")
				ret += " class=\"ok\">";
			else if (status == "error")
				ret += " class=\"wa\">";
			else if (status == "c_error")
				ret += " class=\"tl-rte\">";
			else if (status == "judge_error")
				ret += " class=\"judge-error\">";
			else
				ret += ">";

			return back_insert(ret, submissionStatusDescription(status),
				"</td>");
		};

		string current_date = date("%Y-%m-%d %H:%M:%S");
		while (res.next()) {
			append("<tr>");

			bool admin_view = (res[13] == Session::user_id
				|| Session::user_type < digitsToU<uint>(res[14])
				|| Session::user_id == SIM_ROOT_UID);
			// Rest
			append("<td><a href=\"/s/", res[1], "\">", res[2], "</a></td>"
					"<td>"
						"<a href=\"/c/", res[3], "\">",
							htmlSpecialChars(res[4]), "</a>"
						" ~> "
						"<a href=\"/c/", res[5], "\">",
							htmlSpecialChars(res[6]), "</a>"
						" ~> "
						"<a href=\"/c/", res[7], "\">",
							htmlSpecialChars(res[8]), "</a>"
					"</td>",
					statusRow(res[9]),
					"<td>", (admin_view || string(res[12]) <= current_date
						? res[10] : ""), "</td>"
					"<td>", (res.getBool(11) ? "Yes" : ""), "</td>"
					"<td>"
						"<a class=\"btn-small\" href=\"/s/", res[1],
							"/source\">View source</a>"
						"<a class=\"btn-small\" href=\"/s/", res[1],
							"/download\">Download</a>");

			if (admin_view)
				append("<a class=\"btn-small blue\" href=\"/s/", res[1],
						"/rejudge\">Rejudge</a>"
					"<a class=\"btn-small red\" href=\"/s/", res[1],
						"/delete\">Delete</a>");

			append("</td>"
				"<tr>\n");
		}

		append("</tbody>\n"
			"</table>\n");

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}
}

void User::userSubmissions() {
	userTemplate("User submissions");
	append("<h1>User submissions</h1>");

	printUser();
	printUserSubmissions();
}
