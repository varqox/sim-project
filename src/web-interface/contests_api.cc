#include "sim.h"

void Sim::api_contests() {
	STACK_UNWINDING_MARK;

	using PERM = ContestPermissions;
	using CUM = ContestUserMode;

	session_open();

	InplaceBuff<512> qfields, qwhere;
	qfields.append("SELECT c.id, c.name, c.is_public, cu.mode");
	qwhere.append(" FROM contests c"
		" LEFT JOIN contest_users cu ON cu.contest_id=c.id AND cu.user_id=",
			(session_is_open ? session_user_id : StringView("''")));

	auto qwhere_append = [&, where_was_added = false](auto&&... args) mutable {
		if (where_was_added)
			qwhere.append(" AND ", std::forward<decltype(args)>(args)...);
		else {
			qwhere.append(" WHERE ", std::forward<decltype(args)>(args)...);
			where_was_added = true;
		}
	};

	// Get the overall permissions to the contests list
	contests_perms = contests_get_permissions();
	// Choose contests to select
	if (uint(~contests_perms & PERM::VIEW_ALL)) {
		throw_assert(uint(contests_perms & PERM::VIEW_PUBLIC));
		if (session_is_open)
			qwhere_append("(c.is_public=true OR cu.mode IS NOT NULL)");
		else
			qwhere_append("c.is_public=true");
	}

	// Process restrictions
	StringView next_arg = url_args.extractNextArg();
	for (uint mask = 0; next_arg.size(); next_arg = url_args.extractNextArg()) {
		constexpr uint ID_COND = 1;
		constexpr uint PUBLIC_COND = 2;

		auto arg = decodeURI(next_arg);
		char cond = arg[0];
		StringView arg_id = StringView{arg}.substr(1);

		// Is public
		if (cond == 'p' and ~mask & PUBLIC_COND) {
			if (arg_id == "Y")
				qwhere_append("c.is_public=true");
			else if (arg_id == "N")
				qwhere_append("c.is_public=false");
			else
				return api_error400(concat("Invalid is_public condition: ",
					arg_id));

			mask |= PUBLIC_COND;
			continue;
		}

		if (not isDigit(arg_id))
			return api_error400();

		// conditional
		if (isIn(cond, "=<>") and ~mask & ID_COND) {
			qwhere_append("c.id", arg);
			mask |= ID_COND;

		} else
			return api_error400();

	}

	// Execute query
	qfields.append(qwhere, " ORDER BY c.id DESC LIMIT 50");
	auto res = mysql.query(qfields);

	resp.headers["content-type"] = "text/plain; charset=utf-8";
	append("[");
	while (res.next()) {
		StringView cid = res[0];
		StringView name = res[1];
		bool is_public = strtoull(res[2]);
		CUM umode = (res.is_null(3) ? CUM::IS_NULL : CUM(strtoull(res[3])));

		auto perms = contests_get_permissions(is_public, umode);

		// Id
		append("\n[", cid, ",");
		// Name
		append(jsonStringify(name), ',');
		// Is public
		append(is_public ? "true," : "false,");

		auto user_mode_to_json = [](CUM cum) {
			switch (cum) {
			case CUM::IS_NULL: return "null";
			case CUM::CONTESTANT: return "\"contestant\"";
			case CUM::MODERATOR: return "\"moderator\"";
			case CUM::OWNER: return "\"owner\"";
			}

			return "unknown";
		};

		// Contest user mode
		append(user_mode_to_json(umode));

		// Append what buttons to show
		append(",\"");
		if (uint(perms & (PERM::ADD_PUBLIC | PERM::ADD_PRIVATE)))
			append('a');
		if (uint(perms & PERM::VIEW))
			append('v');
		if (uint(perms & PERM::PARTICIPATE))
			append('p');
		if (uint(perms & PERM::ADMIN))
			append('A');
		if (uint(perms & PERM::EDIT_OWNERS))
			append('O');
		if (uint(perms & PERM::DELETE))
			append('D');
		append("\"],");
	}

	if (resp.content.back() == ',')
		--resp.content.size;

	append("\n]");
}

void Sim::api_contest() {
	STACK_UNWINDING_MARK;

	session_open();
	problems_perms = problems_get_permissions();

	StringView next_arg = url_args.extractNextArg();
	if (next_arg == "add")
		return api_problem_add();
	else if (not isDigit(next_arg))
		return api_error400();

	problems_pid = next_arg;

	InplaceBuff<32> powner;
	InplaceBuff<PROBLEM_LABEL_MAX_LEN> plabel;
	InplaceBuff<1 << 16> simfile;
	uint ptype;

	auto stmt = mysql.prepare("SELECT owner, type, label, simfile FROM problems"
		" WHERE id=?");
	stmt.bindAndExecute(problems_pid);
	stmt.res_bind_all(powner, ptype, plabel, simfile);
	if (not stmt.next())
		return api_error404();

	problems_perms = problems_get_permissions(powner, ProblemType(ptype));

	next_arg = url_args.extractNextArg();
	if (next_arg == "statement")
		return api_problem_statement(plabel, simfile);
	else if (next_arg == "download")
		return api_problem_download(plabel);
	else if (next_arg == "rejudge_all_submissions")
		return api_problem_rejudge_all_submissions();
	else if (next_arg == "reupload")
		return api_problem_reupload();
	// else if (next_arg == "edit")
	// 	return api_problem_edit();
	// else if (next_arg == "delete")
	// 	return api_problem_delete();
	else
		return api_error400();
}
