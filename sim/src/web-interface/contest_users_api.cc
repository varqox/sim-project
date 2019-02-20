#include "sim.h"

using std::pair;

static Sim::ContestUserPermissions
get_overall_perm(Optional<ContestUserMode> viewer_mode) noexcept
{
	using PERM = Sim::ContestUserPermissions;
	using CUM = ContestUserMode;

	if (not viewer_mode.has_value())
		return PERM::NONE;

	switch (viewer_mode.value()) {
	case CUM::OWNER:
		return PERM::ACCESS_API | PERM::ADD_CONTESTANT | PERM::ADD_MODERATOR |
			PERM::ADD_OWNER;
	case CUM::MODERATOR:
		return PERM::ACCESS_API | PERM::ADD_CONTESTANT | PERM::ADD_MODERATOR;
	case CUM::CONTESTANT:
		return PERM::NONE;
	}

	return PERM::NONE; // Shouldn't happen
}

Sim::ContestUserPermissions Sim::contest_user_get_overall_permissions(
	Optional<ContestUserMode> viewer_mode) noexcept
{
	STACK_UNWINDING_MARK;
	using PERM = ContestUserPermissions;
	using CUM = ContestUserMode;

	if (not session_is_open)
		return PERM::NONE;

	if (session_user_type == UserType::ADMIN)
		viewer_mode = CUM::OWNER;

	return get_overall_perm(viewer_mode);
}

Sim::ContestUserPermissions Sim::contest_user_get_permissions(
	Optional<ContestUserMode> viewer_mode, Optional<ContestUserMode> user_mode) noexcept
{
	STACK_UNWINDING_MARK;
	using PERM = ContestUserPermissions;
	using CUM = ContestUserMode;

	if (not session_is_open)
		return PERM::NONE;

	if (session_user_type == UserType::ADMIN)
		viewer_mode = CUM::OWNER;

	if (not viewer_mode.has_value() or not user_mode.has_value())
		return get_overall_perm(viewer_mode);

	switch (viewer_mode.value()) {
	case CUM::OWNER:
		return get_overall_perm(viewer_mode) | PERM::MAKE_CONTESTANT |
			PERM::MAKE_MODERATOR | PERM::MAKE_OWNER | PERM::EXPEL;
	case CUM::MODERATOR: {
		switch (user_mode.value()) {
		case CUM::OWNER: return get_overall_perm(viewer_mode);
		case CUM::MODERATOR:
		case CUM::CONTESTANT:
			return get_overall_perm(viewer_mode) | PERM::MAKE_CONTESTANT |
				PERM::MAKE_MODERATOR | PERM::EXPEL;
		}
		return PERM::NONE; // Shouldn't happen
	}
	case CUM::CONTESTANT:
		return get_overall_perm(viewer_mode);
	}

	return PERM::NONE; // Shouldn't happen
}

pair<Optional<ContestUserMode>, Sim::ContestUserPermissions>
Sim::contest_user_get_overall_permissions(StringView contest_id) {
	STACK_UNWINDING_MARK;

	if (not session_is_open)
		return {std::nullopt, contest_user_get_overall_permissions(std::nullopt)};

	auto stmt = mysql.prepare("SELECT cu.mode FROM contests c"
			" LEFT JOIN contest_users cu ON cu.user_id=?"
				" AND cu.contest_id=c.id"
			" WHERE c.id=?");
	stmt.bindAndExecute(session_user_id, contest_id);

	MySQL::Optional<EnumVal<ContestUserMode>> cu_mode;
	stmt.res_bind_all(cu_mode);
	if (not stmt.next())
		return {std::nullopt, ContestUserPermissions::NONE};

	return {cu_mode, contest_user_get_overall_permissions(cu_mode)};
}

std::tuple<Optional<ContestUserMode>, Sim::ContestUserPermissions,
	Optional<ContestUserMode>>
Sim::contest_user_get_permissions(StringView contest_id, StringView user_id) {
	STACK_UNWINDING_MARK;

	if (not session_is_open)
		return {
			std::nullopt,
			contest_user_get_overall_permissions(std::nullopt),
			std::nullopt
		};

	auto stmt = mysql.prepare("SELECT v.mode, u.mode FROM contests c"
			" LEFT JOIN contest_users v ON v.user_id=?"
				" AND v.contest_id=c.id"
			" LEFT JOIN contest_users u ON u.user_id=?"
				" AND u.contest_id=c.id"
			" WHERE c.id=?");
	stmt.bindAndExecute(session_user_id, user_id, contest_id);

	MySQL::Optional<EnumVal<ContestUserMode>> v_mode;
	MySQL::Optional<EnumVal<ContestUserMode>> u_mode;
	stmt.res_bind_all(v_mode, u_mode);
	if (not stmt.next())
		return {std::nullopt, ContestUserPermissions::NONE, std::nullopt};

	return {
		v_mode,
		contest_user_get_permissions(v_mode, u_mode),
		u_mode
	};
}

void Sim::api_contest_users() {
	STACK_UNWINDING_MARK;
	using CUP = ContestUserPermissions;
	using CUM = ContestUserMode;

	if (not session_is_open)
		return api_error403();

	bool allow_access = false; // Contest argument must occur

	InplaceBuff<512> qfields, qwhere;
	qfields.append("SELECT cu.user_id, u.username, u.first_name, u.last_name,"
		" cu.mode");
	qwhere.append(" FROM contest_users cu"
		" STRAIGHT_JOIN users u ON u.id=cu.user_id"
		" WHERE 1=1");

	enum ColumnIdx {
		UID, USERNAME, FNAME, LNAME, MODE
	};

	auto append_column_names = [&] {
		append("[{\"fields\":["
				"\"overall_actions\","
				"{\"name\":\"rows\", \"columns\":["
					"\"id\","
					"\"username\","
					"\"first_name\","
					"\"last_name\","
					"\"mode\","
					"\"actions\""
				"]}"
			"]}");
	};

	auto set_empty_response = [&] {
		resp.content.clear();
		append_column_names();
		append(",\n\"\",[\n]]"); // Empty overall actions
	};

	// Process restrictions
	auto rows_limit = API_FIRST_QUERY_ROWS_LIMIT;
	CUP overall_perms = CUP::NONE;
	Optional<ContestUserMode> cuser_mode;
	{
		bool mode_condition_occurred = false;
		bool user_id_condition_occurred = false;
		bool contest_id_condition_occurred = false;

		for (StringView next_arg = url_args.extractNextArg();
			not next_arg.empty(); next_arg = url_args.extractNextArg())
		{
			auto arg = decodeURI(next_arg);
			char cond_c = arg[0];
			StringView arg_id = StringView(arg).substr(1);

			// User's mode
			if (cond_c == 'm') {
				if (mode_condition_occurred)
					return api_error400("User mode specified more than once");

				mode_condition_occurred = true;

				if (arg_id == "O")
					qwhere.append(" AND cu.mode=" CU_MODE_OWNER_STR);
				else if (arg_id == "M")
					qwhere.append(" AND cu.mode=" CU_MODE_MODERATOR_STR);
				else if (arg_id == "C")
					qwhere.append(" AND cu.mode=" CU_MODE_CONTESTANT_STR);
				else
					return api_error400(intentionalUnsafeStringView(
						concat("Invalid user mode: ", arg_id)));

			// Next conditions require arg_id to be a valid ID
			} else if (not isDigit(arg_id)) {
				return api_error400();

			// User id
			} else if (isOneOf(cond_c, '<', '>', '=')) {
				if (user_id_condition_occurred)
					return api_error400("User ID condition specified more"
						" than once");

				rows_limit = API_OTHER_QUERY_ROWS_LIMIT;
				user_id_condition_occurred = true;
				qwhere.append(" AND cu.user_id", arg);

			// Contest id
			} else if (cond_c == 'c') {
				if (contest_id_condition_occurred)
					return api_error400("Contest ID condition specified more"
						" than once");

				contest_id_condition_occurred = true;
				qwhere.append(" AND cu.contest_id=", arg_id);

				std::tie(cuser_mode, overall_perms) =
					contest_user_get_overall_permissions(arg_id);

				if (uint(overall_perms & CUP::ACCESS_API))
					allow_access = true;

			} else
				return api_error400();
		}
	}

	if (not allow_access)
		return set_empty_response();

	// Execute query
	auto res = mysql.query(intentionalUnsafeStringView(concat(qfields, qwhere,
		" ORDER BY cu.user_id DESC LIMIT ", rows_limit)));

	append_column_names();

	// Overall actions
	append(",\n\"");
	if (uint(overall_perms & CUP::ADD_CONTESTANT))
		append("Ac");
	if (uint(overall_perms & CUP::ADD_MODERATOR))
		append("Am");
	if (uint(overall_perms & CUP::ADD_OWNER))
		append("Ao");
	append("\",[");

	auto curr_date = mysql_date();
	for (bool first = true; res.next(); ) {
		if (first)
			first = false;
		else
			append(',');

		// User id, username, first name, last name
		append("\n[", res[UID], ',',
			jsonStringify(res[USERNAME]), ',',
			jsonStringify(res[FNAME]), ',',
			jsonStringify(res[LNAME]), ',');

		// Mode
		CUM mode = CUM(strtoull(res[MODE]));
		switch (mode) {
		case CUM::CONTESTANT: append("\"Contestant\","); break;
		case CUM::MODERATOR: append("\"Moderator\","); break;
		case CUM::OWNER: append("\"Owner\","); break;
		}

		CUP perms = contest_user_get_permissions(cuser_mode, mode);
		// Actions
		append("\"");
		if (uint(perms & CUP::MAKE_CONTESTANT))
			append("Mc");
		if (uint(perms & CUP::MAKE_MODERATOR))
			append("Mm");
		if (uint(perms & CUP::MAKE_OWNER))
			append("Mo");
		if (uint(perms & CUP::EXPEL))
			append("E");
		append("\"]");
	}

	append("\n]]");
}

void Sim::api_contest_user() {
	STACK_UNWINDING_MARK;

	if (not session_is_open)
		return api_error403();

	StringView contest_id = url_args.extractNextArg();
	if (contest_id.empty())
		return api_error400("Missing contest id");
	if (contest_id[0] != 'c' or not isDigit(contest_id = contest_id.substr(1)))
		return api_error400("Invalid contest id");

	StringView next_arg = url_args.extractNextArg();
	if (next_arg == "add")
		return api_contest_user_add(contest_id);

	StringView user_id = next_arg;
	if (user_id.empty())
		return api_error400("Missing user id");
	if (user_id[0] != 'u' or not isDigit(user_id = user_id.substr(1)))
		return api_error400("Invalid user id");

	next_arg = url_args.extractNextArg();
	if (next_arg == "change_mode")
		return api_contest_user_change_mode(contest_id, user_id);
	else if (next_arg == "expel")
		return api_contest_user_expel(contest_id, user_id);
	else
		return api_error404();
}


void Sim::api_contest_user_add(StringView contest_id) {
	STACK_UNWINDING_MARK;
	using PERMS = ContestUserPermissions;
	using CUM = ContestUserMode;

	PERMS perms = contest_user_get_overall_permissions(contest_id).second;
	StringView mode_s = request.form_data.get("mode");

	CUM mode;
	PERMS needed_perm;
	if (mode_s == "C")  {
		mode = CUM::CONTESTANT;
		needed_perm = PERMS::ADD_CONTESTANT;
	} else if (mode_s == "M") {
		mode = CUM::MODERATOR;
		needed_perm = PERMS::ADD_MODERATOR;
	} else if (mode_s == "O") {
		mode = CUM::OWNER;
		needed_perm = PERMS::ADD_OWNER;
	} else {
		return add_notification("error",
			"Invalid mode, it must be one of those: C, M, or O");
	}

	StringView user_id;
	form_validate(user_id, "user_id", "User ID", isDigit,
		"User ID: invalid value");

	if (notifications.size)
		return api_error400(notifications);

	if (uint(~perms & needed_perm))
		return api_error403();

	UserPermissions user_perms = users_get_permissions(user_id);
	if (uint(~user_perms & UserPermissions::VIEW))
		return api_error400("Specified user does not exist or you have no"
			" permission to add them");

	auto stmt = mysql.prepare("INSERT IGNORE contest_users(contest_id, user_id, mode)"
		" VALUES(?, ?, ?)");
	stmt.bindAndExecute(contest_id, user_id, EnumVal<CUM>(mode));
	if (stmt.affected_rows() == 0)
		return api_error400("Specified user is already in the contest");
}

void Sim::api_contest_user_change_mode(StringView contest_id, StringView user_id) {
	STACK_UNWINDING_MARK;
	using PERMS = ContestUserPermissions;
	using CUM = ContestUserMode;

	PERMS perms = std::get<1>(contest_user_get_permissions(contest_id, user_id));
	StringView mode_s = request.form_data.get("mode");

	CUM new_mode;
	PERMS needed_perm;
	if (mode_s == "C")  {
		new_mode = CUM::CONTESTANT;
		needed_perm = PERMS::MAKE_CONTESTANT;
	} else if (mode_s == "M") {
		new_mode = CUM::MODERATOR;
		needed_perm = PERMS::MAKE_MODERATOR;
	} else if (mode_s == "O") {
		new_mode = CUM::OWNER;
		needed_perm = PERMS::MAKE_OWNER;
	} else {
		add_notification("error",
			"Invalid mode, it must be one of those: C, M, or O");
	}

	if (notifications.size)
		return api_error400(notifications);

	if (uint(~perms & needed_perm))
		return api_error403();

	auto stmt = mysql.prepare("UPDATE contest_users SET mode=?"
		" WHERE contest_id=? AND user_id=?");
	stmt.bindAndExecute(EnumVal<CUM>(new_mode), contest_id, user_id);
}

void Sim::api_contest_user_expel(StringView contest_id, StringView user_id) {
	STACK_UNWINDING_MARK;
	using PERMS = ContestUserPermissions;

	PERMS perms = std::get<1>(contest_user_get_permissions(contest_id, user_id));
	if (uint(~perms & PERMS::EXPEL))
		return api_error403();

	auto stmt = mysql.prepare("DELETE FROM contest_users"
		" WHERE contest_id=? AND user_id=?");
	stmt.bindAndExecute(contest_id, user_id);
}
