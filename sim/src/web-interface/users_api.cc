#include "sim.h"

#include <sim/utilities.h>
#include <simlib/random.h>
#include <simlib/sha.h>

using std::array;
using std::string;

void Sim::api_users() {
	STACK_UNWINDING_MARK;
	using PERM = UserPermissions;

	if (not session_is_open)
		return api_error403();

	InplaceBuff<256> query;
	query.append("SELECT id, username, first_name, last_name, email, type"
		" FROM users");

	enum ColumnIdx {
		UID, USERNAME, FNAME, LNAME, EMAIL, UTYPE
	};

	auto query_append = [&, where_was_added = false](auto&&... args) mutable {
		if (where_was_added)
			query.append(" AND ", std::forward<decltype(args)>(args)...);
		else {
			query.append(" WHERE ", std::forward<decltype(args)>(args)...);
			where_was_added = true;
		}
	};

	if (session_user_type == UserType::TEACHER)
		query_append("id=", session_user_id);
		// query_append("id!=" SIM_ROOT_UID);
	else if (session_user_type == UserType::NORMAL)
		query_append("id=", session_user_id);

	// Process restrictions
	StringView next_arg = url_args.extractNextArg();
	for (uint mask = 0; next_arg.size(); next_arg = url_args.extractNextArg()) {
		constexpr uint ID_COND = 1;
		constexpr uint UTYPE_COND = 2;

		auto arg = decodeURI(next_arg);
		char cond = arg[0];
		StringView arg_id = StringView{arg}.substr(1);

		// user type
		if (cond == 't' and ~mask & UTYPE_COND) {
			if (arg_id == "A")
				query_append("type=" UTYPE_ADMIN_STR);
			else if (arg_id == "T")
				query_append("type=" UTYPE_TEACHER_STR);
			else if (arg_id == "N")
				query_append("type=" UTYPE_NORMAL_STR);
			else
				return api_error400(concat("Invalid user type: ", arg_id));

			mask |= UTYPE_COND;
			continue;
		}

		if (not isDigit(arg_id))
			return api_error400();

		// conditional
		if (isIn(cond, {'<', '>'}) and ~mask & ID_COND) {
			query_append("id", arg);
			mask |= ID_COND;

		} else if (cond == '=' and ~mask & ID_COND) {
			query_append("id", arg);
			mask |= ID_COND;

		} else
			return api_error400();

	}

	query.append(" ORDER BY id LIMIT 50");
	auto res = mysql.query(query);

	append("[\n{\"columns\":["
			"\"id\","
			"\"username\","
			"\"first_name\","
			"\"last_name\","
			"\"email\","
			"\"type\","
			"\"actions\""
		"]}");

	while (res.next()) {
		append(",\n[", res[UID], ","
			"\"", res[USERNAME], "\",",
			jsonStringify(res[FNAME]), ',',
			jsonStringify(res[LNAME]), ',',
			jsonStringify(res[EMAIL]), ',');

		UserType utype {UserType(strtoull(res[UTYPE]))};
		switch (utype) {
		case UserType::ADMIN: append("\"Admin\","); break;
		case UserType::TEACHER: append("\"Teacher\","); break;
		case UserType::NORMAL: append("\"Normal\","); break;
		}

		// Allowed actions
		append('"');

		auto perms = users_get_permissions(res[UID], utype);
		if (uint(perms & PERM::VIEW))
			append('v');
		if (uint(perms & PERM::EDIT))
			append('E');
		if (uint(perms & PERM::CHANGE_PASS))
			append('p');
		if (uint(perms & PERM::ADMIN_CHANGE_PASS))
			append('P');
		if (uint(perms & PERM::DELETE))
			append('D');
		if (uint(perms & PERM::MAKE_ADMIN))
			append('A');
		if (uint(perms & PERM::MAKE_TEACHER))
			append('T');
		if (uint(perms & PERM::MAKE_NORMAL))
			append('N');

		append("\"]");
	}

	append("\n]");
}

void Sim::api_user() {
	STACK_UNWINDING_MARK;

	if (not session_is_open)
		return api_error403();

	StringView next_arg = url_args.extractNextArg();
	if (next_arg == "add") {
		users_perms = users_get_overall_permissions();
		return api_user_add();

	} else if (not isDigit(next_arg) or
		request.method != server::HttpRequest::POST)
	{
		return api_error400();
	}

	users_uid = next_arg;
	users_perms = users_get_permissions(users_uid);
	if (users_perms == UserPermissions::NONE)
		return api_error404();

	next_arg = url_args.extractNextArg();
	if (next_arg == "edit")
		return api_user_edit();
	else if (next_arg == "delete")
		return api_user_delete();
	else if (next_arg == "change-password")
		return api_user_change_password();
	else
		return api_error404();
}

void Sim::api_user_add() {
	STACK_UNWINDING_MARK;
	using PERM = UserPermissions;

	if (uint(~users_perms & PERM::ADD_USER))
		return api_error403();

	// Validate fields
	StringView pass = request.form_data.get("pass");
	StringView pass1 = request.form_data.get("pass1");

	if (pass != pass1)
		return api_error403("Passwords do not match");

	StringView username, utype_str, fname, lname, email;

	form_validate_not_blank(username, "username", "Username", isUsername,
		"Username can only consist of characters [a-zA-Z0-9_-]",
		USERNAME_MAX_LEN);

	// Validate user type
	utype_str = request.form_data.get("type");
	UserType utype /*= UserType::NORMAL*/;
	if (utype_str == "A") {
		utype = UserType::ADMIN;
		if (uint(~users_perms & PERM::MAKE_ADMIN))
			add_notification("error",
				"You have no permissions to make this user an admin");

	} else if (utype_str == "T") {
		utype = UserType::TEACHER;
		if (uint(~users_perms & PERM::MAKE_TEACHER))
			add_notification("error",
				"You have no permissions to make this user a teacher");

	} else if (utype_str == "N") {
		utype = UserType::NORMAL;
		if (uint(~users_perms & PERM::MAKE_NORMAL))
			add_notification("error",
				"You have no permissions to make this user a normal user");

	} else
		add_notification("error", "Invalid user's type");

	form_validate_not_blank(fname, "first_name", "First Name",
		USER_FIRST_NAME_MAX_LEN);

	form_validate_not_blank(lname, "last_name", "Last Name",
		USER_LAST_NAME_MAX_LEN);

	form_validate_not_blank(email, "email", "Email", USER_EMAIL_MAX_LEN);

	if (notifications.size)
		return api_error400(notifications);

	// All fields are valid
	array<char, (SALT_LEN >> 1)> salt_bin;
	fillRandomly(salt_bin.data(), salt_bin.size());
	auto salt = toHex(salt_bin.data(), salt_bin.size());

	auto stmt = mysql.prepare("INSERT IGNORE `users` (username, type,"
			"first_name, last_name, email, salt, password) "
		"VALUES(?, ?, ?, ?, ?, ?, ?)");

	stmt.bindAndExecute(username, uint(utype), fname, lname, email, salt,
		sha3_512(concat(salt, pass)));

	// User account successfully created
	if (stmt.affected_rows() != 1)
		return api_error400("Username taken");

	stdlog("New user: ", stmt.insert_id(), " -> `", username, '`');
}

void Sim::api_user_edit() {
	STACK_UNWINDING_MARK;
	using PERM = UserPermissions;

	if (uint(~users_perms & PERM::EDIT))
		return api_error403();

	// TODO: this look very similar to the above validation
	StringView username, new_utype_str, fname, lname, email;
	// Validate fields
	form_validate_not_blank(username, "username", "Username", isUsername,
		"Username can only consist of characters [a-zA-Z0-9_-]",
		USERNAME_MAX_LEN);

	// Validate user type
	new_utype_str = request.form_data.get("type");
	UserType new_utype = UserType::NORMAL;
	if (new_utype_str == "A") {
		new_utype = UserType::ADMIN;
		if (uint(~users_perms & PERM::MAKE_ADMIN))
			add_notification("error",
				"You have no permissions to make this user an admin");

	} else if (new_utype_str == "T") {
		new_utype = UserType::TEACHER;
		if (uint(~users_perms & PERM::MAKE_TEACHER))
			add_notification("error",
				"You have no permissions to make this user a teacher");

	} else if (new_utype_str == "N") {
		new_utype = UserType::NORMAL;
		if (uint(~users_perms & PERM::MAKE_NORMAL))
			add_notification("error",
				"You have no permissions to make this user a normal user");

	} else
		add_notification("error", "Invalid user's type");

	form_validate_not_blank(fname, "first_name", "First Name",
		USER_FIRST_NAME_MAX_LEN);

	form_validate_not_blank(lname, "last_name", "Last Name",
		USER_LAST_NAME_MAX_LEN);

	form_validate_not_blank(email, "email", "Email", USER_EMAIL_MAX_LEN);

	if (notifications.size)
		return api_error400(notifications);

	// Commit changes
	auto stmt = mysql.prepare("UPDATE IGNORE users"
		" SET username=?, first_name=?, last_name=?, email=?, type=?"
		" WHERE id=?");
	stmt.bindAndExecute(username, fname, lname, email, uint(new_utype),
		users_uid);

	if (stmt.affected_rows() == 0 and mysql_warning_count(mysql))
		return api_error400("Username is already taken");
}

void Sim::api_user_delete() {
	STACK_UNWINDING_MARK;
	using PERM = UserPermissions;

	if (uint(~users_perms & PERM::DELETE))
		return api_error403();

	auto stmt = mysql.prepare("SELECT salt, password FROM users WHERE id=?");
	stmt.bindAndExecute(session_user_id);

	InplaceBuff<SALT_LEN> salt;
	InplaceBuff<PASSWORD_HASH_LEN> passwd_hash;
	stmt.res_bind_all(salt, passwd_hash);
	throw_assert(stmt.next());

	if (not slowEqual(sha3_512(concat(salt, request.form_data.get("password"))),
		passwd_hash))
	{
		return api_error403("Invalid password");
	}

	// TODO: make a job out of it
	api_error403("It is disabled for now because it is not safe");

	// TODO: add other things like problems, contest_users, contests, messages, files etc.
	stmt = mysql.prepare("DELETE FROM submissions WHERE owner=?");
	stmt.bindAndExecute(users_uid);

	stmt = mysql.prepare("DELETE FROM users WHERE id=?");
	stmt.bindAndExecute(users_uid);
}

void Sim::api_user_change_password() {
	STACK_UNWINDING_MARK;
	using PERM = UserPermissions;

	if (uint(~users_perms & PERM::CHANGE_PASS) and
		uint(~users_perms & PERM::ADMIN_CHANGE_PASS))
	{
		return api_error403();
	}

	StringView old_pass = request.form_data.get("old_pass");
	StringView new_pass = request.form_data.get("new_pass");
	StringView new_pass1 = request.form_data.get("new_pass1");

	if (new_pass != new_pass1)
		return api_error400("Passwords do not match");

	// Get salt and the hash of the password
	InplaceBuff<SALT_LEN> salt;
	InplaceBuff<PASSWORD_HASH_LEN> passwd_hash;
	auto stmt = mysql.prepare("SELECT salt, password FROM users WHERE id=?");
	stmt.bindAndExecute(users_uid);
	stmt.res_bind_all(salt, passwd_hash);
	if (!stmt.next())
		THROW("Cannot get user's password");

	if (uint(~users_perms & PERM::ADMIN_CHANGE_PASS) and
		not slowEqual(sha3_512(concat(salt, old_pass)), passwd_hash))
	{
		return api_error403("Invalid old password");
	}

	// Commit password change
	char salt_bin[SALT_LEN >> 1];
	fillRandomly(salt_bin, sizeof(salt_bin));
	salt = toHex(salt_bin, sizeof(salt_bin));

	stmt = mysql.prepare("UPDATE users SET salt=?, password=? WHERE id=?");
	stmt.bindAndExecute(salt, sha3_512(concat(salt, new_pass)), users_uid);
}
