#include "sim.h"

void Sim::api_user() {
	STACK_UNWINDING_MARK;

	return api_error400();
}

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
		if (uint(perms & PERM::DEMOTE))
			append('d');

		append("\"],");
	}

	if (resp.content.back() == ',')
		--resp.content.size;

	append("\n]");
}
