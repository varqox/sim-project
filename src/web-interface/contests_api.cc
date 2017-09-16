#include "sim.h"

void Sim::append_contest_actions_str() {
	STACK_UNWINDING_MARK;

	using PERM = ContestPermissions;

	append('"');
	if (uint(contests_perms & (PERM::ADD_PUBLIC | PERM::ADD_PRIVATE)))
		append('a');
	if (uint(contests_perms & PERM::VIEW))
		append('v');
	if (uint(contests_perms & PERM::PARTICIPATE))
		append('p');
	if (uint(contests_perms & PERM::ADMIN))
		append('A');
	if (uint(contests_perms & PERM::EDIT_OWNERS))
		append('O');
	if (uint(contests_perms & PERM::DELETE))
		append('D');
	append('"');
}

static constexpr inline const char* user_mode_to_json(ContestUserMode cum) {
	switch (cum) {
	case ContestUserMode::IS_NULL: return "null";
	case ContestUserMode::CONTESTANT: return "\"contestant\"";
	case ContestUserMode::MODERATOR: return "\"moderator\"";
	case ContestUserMode::OWNER: return "\"owner\"";
	}

	return "unknown";
};

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

		contests_perms = contests_get_permissions(is_public, umode);

		// Id
		append("\n[", cid, ",");
		// Name
		append(jsonStringify(name), ',');
		// Is public
		append(is_public ? "true," : "false,");

		// Contest user mode
		append(user_mode_to_json(umode));

		// Append what buttons to show
		append(',');
		append_contest_actions_str();
		append("],");
	}

	if (resp.content.back() == ',')
		--resp.content.size;

	append("\n]");
}

void Sim::api_contest() {
	STACK_UNWINDING_MARK;

	using PERM = ContestPermissions;
	using CUM = ContestUserMode;

	session_open();
	problems_perms = problems_get_permissions();

	StringView next_arg = url_args.extractNextArg();
	if (next_arg == "add") {
		return api_contest_add();

	} else if (next_arg.empty()) {
		return api_error400();

	// Select by contest id
	} else if (next_arg[0] == 'c' and isDigit(next_arg.substr(1))) {
		contests_cid = next_arg.substr(1);
		auto stmt = mysql.prepare("SELECT c.name, c.is_public, cu.mode"
			" FROM contests c"
			" LEFT JOIN contest_users cu ON cu.contest_id=c.id AND cu.user_id=?"
			" WHERE c.id=?");
		stmt.bindAndExecute(
			(session_is_open ? session_user_id : StringView("")), contests_cid);

		InplaceBuff<meta::max(CONTEST_NAME_MAX_LEN, CONTEST_ROUND_NAME_MAX_LEN,
			CONTEST_PROBLEM_NAME_MAX_LEN)> name;
		std::underlying_type_t<CUM> umode_u;
		bool is_public;
		stmt.res_bind_all(name, is_public, umode_u);
		if (not stmt.next())
			return api_error404();

		CUM umode = (stmt.is_null(2) ? CUM::IS_NULL : CUM(umode_u));
		contests_perms = contests_get_permissions(is_public, umode);

		next_arg = url_args.extractNextArg();
		if (next_arg == "add_round")
			return api_contest_round_add();
		else if (not next_arg.empty())
			return api_error400();

		if (uint(~contests_perms & PERM::VIEW))
			return api_error403();

		// Append contest
		append("[\n[", contests_cid, ',',
			jsonStringify(name),
			(is_public ? ",true," : ",false,"),
			user_mode_to_json(umode), ',');
		append_contest_actions_str();

		decltype(date()) curr_date;

		// Rounds
		append("],\n[");
		if (uint(contests_perms & PERM::ADMIN)) {
			stmt = mysql.prepare("SELECT id, name, item, ranking_exposure,"
					" begins, full_results, ends FROM contest_rounds"
				" WHERE contest_id=?");
			stmt.bindAndExecute(contests_cid);
		} else {
			stmt = mysql.prepare("SELECT id, name, item, ranking_exposure,"
					" begins, full_results, ends FROM contest_rounds"
				" WHERE contest_id=? AND begins<=?");
			curr_date = date();
			stmt.bindAndExecute(contests_cid, curr_date);
		}

		uint item;
		InplaceBuff<20> ranking_exposure, begins, full_results, ends;
		stmt.res_bind_all(contests_crid, name, item, ranking_exposure, begins,
			full_results, ends);

		while (stmt.next()) {
			append("\n[", contests_crid, ',',
				jsonStringify(name), ',',
				item, ',');

			if (stmt.is_null(3))
				append("null,");
			else
				append('"', ranking_exposure, "\",");

			append('"', begins, "\",");

			if (stmt.is_null(5))
				append("null,");
			else
				append('"', full_results, "\",");

			if (stmt.is_null(6))
				append("null],");
			else
				append('"', ends, "\"],");
		}

		if (resp.content.back() == ',')
			--resp.content.size;

		// Problems
		append("\n],\n[");
		if (uint(contests_perms & PERM::ADMIN)) {
			stmt = mysql.prepare("SELECT id, contest_round_id, problem_id,"
				" name, item, final_selecting_method FROM contest_problems"
				" WHERE contest_id=?");
			stmt.bindAndExecute(contests_cid);

		} else {
			stmt = mysql.prepare("SELECT cp.id, cp.contest_round_id,"
					" cp.problem_id, cp.name, cp.item,"
					" cp.final_selecting_method"
				" FROM contest_problems cp"
				" JOIN contest_rounds cr ON cr.id=cp.contest_round_id"
					" AND cr.begins<=?"
				" WHERE cp.contest_id=?");
			stmt.bindAndExecute(curr_date, contests_cid);
		}

		uint64_t problem_id;
		uint final_selecting_method;
		stmt.res_bind_all(contests_cpid, contests_crid, problem_id, name, item,
			final_selecting_method);

		while (stmt.next()) {
			append("\n[", contests_cpid, ',',
				contests_crid, ',',
				problem_id, ',',
				jsonStringify(name), ',',
				item, ',',
				final_selecting_method, "],");
		}

		if (resp.content.back() == ',')
			--resp.content.size;
		append("\n]\n]");


	// Select by contest round id
	} else if (next_arg[0] == 'r' and isDigit(next_arg.substr(1))) {
		contests_crid = next_arg.substr(1);
		auto stmt = mysql.prepare("SELECT c.id, c.is_public, cr.begins, cu.mode"
			" FROM contest_rounds cr "
			" STRAIGHT_JOIN contests c ON c.id=cr.contest_id"
			" LEFT JOIN contest_users cu ON cu.contest_id=c.id AND cu.user_id=?"
			" WHERE cr.id=?");

		stmt.bindAndExecute((session_is_open ? session_user_id : StringView()),
			contests_crid);

		bool is_public;
		InplaceBuff<20> cr_begins;
		std::underlying_type_t<CUM> umode_u;
		stmt.res_bind_all(contests_cid, is_public, cr_begins, umode_u);
		if (not stmt.next())
			return api_error404();

		contests_perms = contests_get_permissions(is_public,
			(stmt.is_null(3) ? CUM::IS_NULL : CUM(umode_u)));

		if (uint(~contests_perms & PERM::VIEW))
			return api_error403(); // Could not participate

		if (cr_begins > date() and uint(~contests_perms & PERM::ADMIN))
			return api_error403(); // Round has not begun jet

		next_arg = url_args.extractNextArg();
		if (next_arg == "add_problem")
			return api_contest_problem_add();
		else
			return error404();

	// Select by contest problem id
	} else if (next_arg[0] == 'p' and isDigit(next_arg.substr(1))) {
		contests_cpid = next_arg.substr(1);
		// Check permissions to the contest
		auto stmt = mysql.prepare("SELECT c.id, c.is_public, cr.id, cr.begins,"
				" cp.problem_id, cu.mode"
			" FROM contest_problems cp"
			" STRAIGHT_JOIN contest_rounds cr ON cr.id=cp.contest_round_id"
			" STRAIGHT_JOIN contests c ON c.id=cp.contest_id"
			" LEFT JOIN contest_users cu ON cu.contest_id=c.id AND cu.user_id=?"
			" WHERE cp.id=?");
		stmt.bindAndExecute((session_is_open ? session_user_id : StringView()),
			contests_cpid);

		bool is_public;
		InplaceBuff<20> cr_begins;
		InplaceBuff<32> problem_id;
		std::underlying_type_t<CUM> umode_u;
		stmt.res_bind_all(contests_cid, is_public, contests_crid, cr_begins,
			problem_id, umode_u);
		if (not stmt.next())
			return api_error404();

		contests_perms = contests_get_permissions(is_public,
			(stmt.is_null(5) ? CUM::IS_NULL : CUM(umode_u)));

		if (uint(~contests_perms & PERM::VIEW))
			return api_error403(); // Could not participate

		if (cr_begins > date() and uint(~contests_perms & PERM::ADMIN))
			return api_error403(); // Round has not begun jet

		next_arg = url_args.extractNextArg();
		if (next_arg == "statement")
			return api_contest_problem_statement(problem_id);
		else
			return error404();

	} else
		return api_error404();
}

void Sim::api_contest_add() {
	STACK_UNWINDING_MARK;

	using PERM = ContestPermissions;

	if (uint(~contests_perms & (PERM::ADD_PRIVATE | PERM::ADD_PUBLIC)))
		return api_error403();

	// Validate fields
	StringView name;
	form_validate_not_blank(name, "name", "Contest's name",
		CONTEST_NAME_MAX_LEN);

	bool is_public = request.form_data.exist("public");
	if (is_public and uint(~contests_perms & PERM::ADD_PUBLIC))
		return api_error403("You have no permissions to add a public contest");

	if (not is_public and uint(~contests_perms & PERM::ADD_PRIVATE))
		return api_error403("You have no permissions to add a private contest");

	if (form_validation_error)
		return api_error400(notifications);

	// Add contest
	auto stmt = mysql.prepare("INSERT contests(name, is_public) VALUES(?, ?)");
	stmt.bindAndExecute(name, is_public);

	auto contest_id = stmt.insert_id();
	// Add user to owners
	stmt = mysql.prepare("INSERT contest_users(user_id, contest_id, mode)"
		" VALUES(?, ?, " CU_MODE_OWNER_STR ")");
	stmt.bindAndExecute(session_user_id, contest_id);

	append(contest_id);
}

void Sim::api_contest_round_add() {
	STACK_UNWINDING_MARK;

	using PERM = ContestPermissions;

	if (uint(~contests_perms & PERM::ADMIN))
		return api_error403();

	// Validate fields
	CStringView name, begins, ends, full_results;
	form_validate_not_blank(name, "name", "Round's name",
		CONTEST_ROUND_NAME_MAX_LEN);
	form_validate(begins, "begins", "Begin time", isDatetime,
		"Begin time: invalid value");
	form_validate(ends, "ends", "End time", isDatetime,
		"End time: invalid value");
	form_validate(full_results, "full_results", "Full results time", isDatetime,
		"Full results time: invalid value");

	// Add round
	auto curr_date = date();
	auto stmt = mysql.prepare("INSERT contest_rounds(contest_id, name, item,"
			" begins, ends, full_results, ranking_exposure)"
		" SELECT ?, ?, COALESCE(MAX(item)+1, 0), ?, ?, ?, ?"
		" FROM contest_rounds WHERE contest_id=?");
	stmt.bind(0, contests_cid);
	stmt.bind(1, name);

	if (begins.empty())
		stmt.bind(2, curr_date);
	else
		stmt.bind(2, begins);

	if (ends.empty())
		stmt.bind(3, nullptr);
	else
		stmt.bind(3, ends);

	if (full_results.empty())
		stmt.bind(4, nullptr);
	else
		stmt.bind(4, full_results);

	stmt.bind(5, curr_date);
	stmt.bind(6, contests_cid);
	stmt.fixAndExecute();

	append(stmt.insert_id());
}

void Sim::api_contest_problem_add() {
	STACK_UNWINDING_MARK;

	using PERM = ContestPermissions;

	if (uint(~contests_perms & PERM::ADMIN))
		return api_error403();

	// Validate fields
	StringView name, problem_id;
	form_validate(name, "name", "Problem's name",
		CONTEST_PROBLEM_NAME_MAX_LEN);
	form_validate(problem_id, "problem_id", "Problem ID", isDigit,
		"Problem ID: invalid value");

	auto stmt = mysql.prepare("SELECT owner, type, name FROM problems"
		" WHERE id=?");
	stmt.bindAndExecute(problem_id);

	// TODO: check problem permissions
	InplaceBuff<32> powner;
	std::underlying_type_t<ProblemType> ptype_u;
	InplaceBuff<PROBLEM_NAME_MAX_LEN> pname;
	stmt.res_bind_all(powner, ptype_u, pname);
	if (not stmt.next())
		return api_error404(concat("No problem was found with ID = ",
			problem_id));

	auto pperms = problems_get_permissions(powner, ProblemType(ptype_u));
	if (uint(~pperms & ProblemPermissions::VIEW))
		return api_error403("You have no permissions to use this problem");

	// Add problem
	stmt = mysql.prepare("INSERT contest_problems(contest_round_id, contest_id,"
			" problem_id, name, item, final_selecting_method)"
		" SELECT ?, ?, ?, ?, COALESCE(MAX(item)+1, 0), 0 FROM contest_problems"
		" WHERE contest_round_id=?");
	stmt.bindAndExecute(contests_crid, contests_cid, problem_id,
		(name.empty() ? pname : name), contests_crid);

	append(stmt.insert_id());
}

void Sim::api_contest_problem_statement(StringView problem_id) {
	STACK_UNWINDING_MARK;

	auto stmt = mysql.prepare("SELECT label, simfile FROM problems WHERE id=?");
	stmt.bindAndExecute(problem_id);

	InplaceBuff<PROBLEM_LABEL_MAX_LEN> label;
	InplaceBuff<1 << 16> simfile;
	stmt.res_bind_all(label, simfile);
	if (not stmt.next())
		return api_error404();

	return api_statement_impl(problem_id, label, simfile);
}
