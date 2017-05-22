#include "sim.h"

#include <sim/utilities.h>

using std::string;

void Sim::api_users() {
	STACK_UNWINDING_MARK;
	using PERM = UserPermissions;

	if (not session_open())
		return api_error403();

	bool allow_access =
		uint(users_get_viewer_permissions(session_user_id, session_user_type) &
			PERM::VIEW_ALL);

	InplaceBuff<256> query;
	query.append("SELECT id, username, first_name, last_name, email, type"
		" FROM users");

	bool where_added = false;
	if (session_user_type != UserType::ADMIN) {
		where_added = true;
		query.append(" WHERE id!=" SIM_ROOT_UID);
	}

	auto arg = decodeURI(url_args.extractNextArg());
	if (arg.size) {
		char cond = arg[0];
		StringView arg_id = StringView{arg}.substr(1);

		if (not isDigit(arg_id))
			return api_error400();

		if (cond == '=') {
			if (not allow_access)
				// Allow selecting the user's data
				allow_access = (arg_id == session_user_id);
		} else if (cond != '>')
			return api_error400();

		if (where_added)
			query.append(" AND id", arg);
		else
			query.append(" WHERE id", arg);
	}

	if (not allow_access)
		return api_error403();

	query.append(" ORDER BY id LIMIT 50");
	auto res = mysql.query(query);

	resp.headers["content-type"] = "text/plain; charset=utf-8";
	append("[");

	while (res.next()) {
		append("\n[", res[0], ',', // id
			jsonStringify(res[1]), ',', // username
			jsonStringify(res[2]), ',', // first_name
			jsonStringify(res[3]), ',', // last_name
			jsonStringify(res[4]), ','); // email

		UserType utype {UserType(strtoull(res[5]))};
		switch (utype) {
		case UserType::ADMIN: append("\"admin\","); break;
		case UserType::TEACHER: append("\"teacher\","); break;
		case UserType::NORMAL: append("\"normal\","); break;
		}

		// Allowed actions
		UserPermissions perms = users_get_viewer_permissions(res[0], utype);
		append('"');

		if (uint(perms & PERM::VIEW))
			append('v');
		if (uint(perms & PERM::EDIT))
			append('E');
		if (uint(perms & PERM::CHANGE_PASS))
			append('P');
		if (uint(perms & PERM::DELETE))
			append('D');
		if (uint(perms & PERM::MAKE_ADMIN))
			append('A');
		if (uint(perms & PERM::MAKE_TEACHER))
			append('T');
		if (uint(perms & PERM::MAKE_NORMAL))
			append('N');

		append("\"],");
	}

	if (resp.content.back() == ',')
		--resp.content.size;

	append("\n]");
}

void Sim::api_user() {
	STACK_UNWINDING_MARK;

	if (not session_open())
		return error403(); // Intentionally the whole template

	users_user_id = url_args.extractNextArg();
	if (not isDigit(users_user_id) or
		request.method != server::HttpRequest::POST)
	{
		return api_error400();
	}

	uint utype;
	auto stmt = mysql.prepare("SELECT type FROM users WHERE id=?");
	stmt.bindAndExecute(users_user_id);
	stmt.res_bind_all(utype);
	if (not stmt.next())
		return api_error404();

	users_user_type = UserType(utype);
	users_perms = users_get_viewer_permissions(users_user_id, users_user_type);

	StringView next_arg = url_args.extractNextArg();
	if (next_arg == "edit")
		return api_user_edit();
	else if (next_arg == "delete")
		return api_user_delete();
	else if (next_arg == "change-password")
		return api_user_change_password();
	else
		return api_error404();
}

void Sim::api_user_edit() {
	STACK_UNWINDING_MARK;
	using PERM = UserPermissions;

	if (uint(~users_perms & PERM::EDIT))
		return api_error403();

	StringView username, new_utype_str, fname, lname, email;
	// Validate fields
	form_validate_not_blank(username, "username", "Username", isUsername,
		"Username can only consist of characters [a-zA-Z0-9_-]",
		USERNAME_MAX_LEN);

	// Validate user type
	form_validate_not_blank(new_utype_str, "type", "User's type");
	UserType new_utype /*= UserType::NORMAL*/;
	if (new_utype_str == "A") {
		new_utype = UserType::ADMIN;
		if (uint(~users_perms & PERM::MAKE_ADMIN))
			return api_error403("You have no permissions to make this user an"
				" admin");

	} else if (new_utype_str == "T") {
		new_utype = UserType::TEACHER;
		if (uint(~users_perms & PERM::MAKE_TEACHER))
			return api_error403("You have no permissions to make this user a"
				" teacher");

	} else if (new_utype_str == "N") {
		new_utype = UserType::NORMAL;
		if (uint(~users_perms & PERM::MAKE_NORMAL))
			return api_error403("You have no permissions to make this user a"
				" normal user");

	} else
		return api_error400("Invalid user's type");

	form_validate_not_blank(fname, "first_name", "First Name",
		USER_FIRST_NAME_MAX_LEN);

	form_validate_not_blank(lname, "last_name", "Last Name",
		USER_LAST_NAME_MAX_LEN);

	form_validate_not_blank(email, "email", "Email", USER_EMAIL_MAX_LEN);

	if (form_validation_error)
		return api_error400(concat("<div>", notifications, "</div>"));

	// Commit changes
	auto stmt = mysql.prepare("UPDATE users"
		" SET username=?, first_name=?, last_name=?, email=?, type=?"
		" WHERE id=?");
	try {
		stmt.bindAndExecute(username, fname, lname, email, uint(new_utype),
			users_user_id);
	} catch (const std::exception&) {
		return api_error400("Username is already taken");
	}
}

void Sim::api_user_delete() {
	STACK_UNWINDING_MARK;
	using PERM = UserPermissions;

	if (uint(~users_perms & PERM::DELETE))
		return api_error403();
}

void Sim::api_user_change_password() {
	STACK_UNWINDING_MARK;
	using PERM = UserPermissions;

	if (uint(~users_perms & PERM::CHANGE_PASS))
		return api_error403();
}
