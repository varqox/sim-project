#include "sim.h"

using std::string;

void Sim::api_submissions() {
	STACK_UNWINDING_MARK;
	using PERM = SubmissionPermissions;
	using CUM = ContestUserMode;

	if (not session_open())
		return api_error403();

	StringView next_arg = url_args.extractNextArg();
	bool select_report = false;

	InplaceBuff<512> qfields, qwhere;
	qfields.append("SELECT s.id, s.type, s.owner, c.owner, cu.mode,"
		" u.username, s.problem_id, p.name, s.round_id, r.name,"
		" s.parent_round_id, pr.name, pr.reveal_score, pr.full_results,"
		" s.contest_round_id, c.name, s.submit_time, s.status, s.score");
	qwhere.append(" FROM submissions s"
		" LEFT JOIN users u ON u.id=s.owner"
		" STRAIGHT_JOIN problems p ON p.id=s.problem_id"
		" LEFT JOIN rounds r ON r.id=s.round_id"
		" LEFT JOIN rounds pr ON pr.id=s.parent_round_id"
		" LEFT JOIN rounds c ON c.id=s.contest_round_id"
		" LEFT JOIN contests_users cu ON cu.contest_id=s.contest_round_id AND"
			" cu.user_id=", session_user_id,
		" WHERE s.type!=" STYPE_VOID_STR);

	bool allow_access = (session_user_type == UserType::ADMIN);

	// Process restrictions
	for (uint mask = 0; next_arg.size(); next_arg = url_args.extractNextArg()) {
		constexpr uint ID_COND = 1;
		constexpr uint STYPE_COND = 2;
		constexpr uint ROUND_OR_PROBLEM_ID_COND = 4;
		constexpr uint USER_ID_COND = 8;

		auto arg = decodeURI(next_arg);
		char cond = arg[0];
		StringView arg_id = StringView{arg}.substr(1);

		// submission type
		if (cond == 't' and ~mask & STYPE_COND) {
			if (arg_id == "N")
				qwhere.append(" AND s.type=" STYPE_NORMAL_STR);
			else if (arg_id == "F")
				qwhere.append(" AND s.type=" STYPE_FINAL_STR);
			else if (arg_id == "I")
				qwhere.append(" AND s.type=" STYPE_IGNORED_STR);
			else if (arg_id == "S")
				qwhere.append(" AND s.type=" STYPE_PROBLEM_SOLUTION_STR);
			else
				return api_error400(concat("Invalid submission type: ",
					arg_id));

			mask |= STYPE_COND;
			continue;
		}

		if (not isDigit(arg_id))
			return api_error400();

		// conditional
		if (cond == '<' and ~mask & ID_COND) {
			qwhere.append(" AND s.id", arg);
			mask |= ID_COND;

		} else if (cond == '=' and ~mask & ID_COND) {
			allow_access = true; // Permissions will be checked on fetching data

			qfields.append(", s.initial_report, s.final_report");
			select_report = true;

			qwhere.append(" AND s.id", arg);
			mask |= ID_COND;

		// problem's or round's id
		} else if (isIn(cond, "pCRP") and ~mask & ROUND_OR_PROBLEM_ID_COND) {
			if (cond == 'p')
				qwhere.append(" AND s.problem_id=", arg_id);
			else {
				if (not allow_access) {
					StringView query;
					switch (cond) {
					case 'C': query = "SELECT c.owner, cu.mode FROM rounds c"
						" LEFT JOIN contests_users cu ON cu.contest_id=c.id"
							" AND cu.user_id=?"
						" WHERE c.id=?";
						break;

					case 'R': query = "SELECT c.owner, cu.mode FROM rounds r"
						" STRAIGHT_JOIN rounds c ON c.id=r.parent"
						" LEFT JOIN contests_users cu ON cu.contest_id=c.id"
							" AND cu.user_id=?"
						" WHERE r.id=?";
						break;

					case 'P': query = "SELECT c.owner, cu.mode FROM rounds r"
						" STRAIGHT_JOIN rounds c ON c.id=r.grandparent"
						" LEFT JOIN contests_users cu ON cu.contest_id=c.id"
							" AND cu.user_id=?"
						" WHERE r.id=?";
						break;
					}
					auto stmt = mysql.prepare(query);
					stmt.bindAndExecute(session_user_id, arg_id);

					InplaceBuff<30> c_owner;
					uint cu_mode;
					stmt.res_bind_all(c_owner, cu_mode);
					if (not stmt.next())
						return api_error404(concat("Invalid query condition ",
							cond, " - no such round was found"));

					if (c_owner == session_user_id or
						(not stmt.isNull(1) and
							cu_mode == (uint)CUM::MODERATOR))
					{
						allow_access = true;
					}
				}

				if (cond == 'C')
					qwhere.append(" AND s.contest_round_id=", arg_id);
				else if (cond == 'R')
					qwhere.append(" AND s.parent_round_id=", arg_id);
				else if (cond == 'P')
					qwhere.append(" AND s.round_id=", arg_id);
			}

			mask |= ROUND_OR_PROBLEM_ID_COND;

		// user (creator)
		} else if (cond == 'u' and ~mask & USER_ID_COND) {
			qwhere.append(" AND s.owner=", arg_id);
			if (arg_id == session_user_id)
				allow_access = true;

			mask |= USER_ID_COND;

		} else
			return api_error400();

	}

	if (not allow_access)
		return api_error403();


	// Execute query
	qfields.append(qwhere, " ORDER BY s.id DESC LIMIT 50");
	stdlog(qfields);
	auto res = mysql.query(qfields);

	resp.headers["content-type"] = "text/plain; charset=utf-8";
	append("[");

	// FINISHED HERE!
	while (res.next()) {
		StringView sid = res[0];
		SubmissionType stype = SubmissionType(strtoull(res[1]));
		StringView sowner = res[2];
		StringView c_owner = (res.isNull(3) ? "" : res[3]);
		StringView cu_mode = res[4];

		SubmissionPermissions perms = submission_get_permissions(sowner,
			c_owner, (res.isNull(4) ? CUM::IS_NULL : CUM(strtoull(cu_mode))));

		if (perms == PERM::NONE)
			return api_error403();


		StringView sowner_username = res[5];
		StringView p_id = res[6];
		StringView p_name = res[7];
		StringView r_id = res[8];
		StringView r_name = res[9];
		StringView pr_id = res[10];
		StringView pr_name = res[11];
		bool reveal_score = (res[12] != "0");
		bool show_full_results = (bool(uint(perms & PERM::VIEW_FULL_REPORT)) or
			res.isNull(13) or res[13] <= date());
		StringView c_id = res[14];
		StringView c_name = res[15];
		StringView submit_time = res[16];
		SubmissionStatus status = SubmissionStatus(strtoull(res[17]));
		StringView score = res[18];

		append("\n[", sid, ',');

		// Type
		switch (stype) {
		case SubmissionType::NORMAL: append("\"Normal\","); break;
		case SubmissionType::FINAL: append("\"Final\","); break;
		case SubmissionType::VOID: throw_assert(false); break;
		case SubmissionType::IGNORED: append("\"Ignored\","); break;
		case SubmissionType::PROBLEM_SOLUTION: append("\"Problem solution\",");
			break;
		}

		// Onwer's id
		if (res.isNull(2))
			append("null,");
		else
			append(sowner, ',');

		// Onwer's username
		if (res.isNull(5))
			append("null,");
			append("\"", sowner_username, "\",");

		// Problem
		if (res.isNull(6))
			append("null,null,");
		else
			append(p_id, ',', jsonStringify(p_name), ',');

		// Rounds
		if (res.isNull(8))
			append("null,null,null,null,null,null,");
		else {
			append(r_id, ',',
				jsonStringify(r_name), ',',
				pr_id, ',',
				jsonStringify(pr_name), ',',
				c_id, ',',
				jsonStringify(c_name), ',');
		}

		// Submit time
		append("\"", submit_time, "\",");

		/* Status: (CSS class, text) */
		using SS = SubmissionStatus;

		// Fatal
		if (status >= SS::PENDING) {
			if (status == SS::PENDING)
				append("[\"status\",\"Pending\"],");
			else if (status == SS::COMPILATION_ERROR)
				append("[\"status purple\",\"Compilation failed\"],");
			else if (status == SS::CHECKER_COMPILATION_ERROR)
				append("[\"status blue\",\"Checker compilation failed\"],");
			else if (status == SS::JUDGE_ERROR)
				append("[\"status blue\",\"Judge error\"],");
			else
				append("[\"status\",\"Unknown\"],");

		// Final
		} else if (show_full_results) {
			if ((status & SS::FINAL_MASK) == SS::OK)
				append("[\"status green\",\"OK\"],");
			else if ((status & SS::FINAL_MASK) == SS::WA)
				append("[\"status red\",\"Wrong answer\"],");
			else if ((status & SS::FINAL_MASK) == SS::TLE)
				append("[\"status yellow\",\"Time limit exceeded\"],");
			else if ((status & SS::FINAL_MASK) == SS::MLE)
				append("[\"status yellow\",\"Memory limit exceeded\"],");
			else if ((status & SS::FINAL_MASK) == SS::RTE)
				append("[\"status intense-red\",\"Runtime error\"],");
			else
				append("[\"status\",\"Unknown\"],");

		// Initial
		} else {
			if ((status & SS::INITIAL_MASK) == SS::INITIAL_OK)
				append("[\"status initial green\",\"OK\"],");
			else if ((status & SS::INITIAL_MASK) == SS::INITIAL_WA)
				append("[\"status initial red\",\"Wrong answer\"],");
			else if ((status & SS::INITIAL_MASK) == SS::INITIAL_TLE)
				append("[\"status initial yellow\",\"Time limit exceeded\"],");
			else if ((status & SS::INITIAL_MASK) == SS::INITIAL_MLE)
				append("[\"status initial yellow\",\"Memory limit exceeded\"],")
					;
			else if ((status & SS::INITIAL_MASK) == SS::INITIAL_RTE)
				append("[\"status initial intense-red\",\"Runtime error\"],");
			else
				append("[\"status\",\"Unknown\"],");
		}

		// Score
		if (not res.isNull(18) and (show_full_results or reveal_score))
			append(score, ',');
		else
			append("null,");

		// Actions
		append('"');

		if (uint(perms & PERM::VIEW))
			append('v');
		if (uint(perms & PERM::VIEW_SOURCE))
			append('s');
		if (uint(perms & PERM::CHANGE_TYPE))
			append('C');
		if (uint(perms & PERM::REJUDGE))
			append('R');
		if (uint(perms & PERM::DELETE))
			append('D');

		append("\"");

		// Append reports
		if (select_report) {
			append(',', jsonStringify(res[19]));
			if (show_full_results)
				append(',', jsonStringify(res[20]));
			else
				append(",null");
		}

		append("],");
	}

	if (resp.content.back() == ',')
		--resp.content.size;

	append("\n]");
}

void Sim::api_submission() {
	STACK_UNWINDING_MARK;
	using CUM = ContestUserMode;

	if (not session_open())
		return error403(); // Intentionally the whole template

	StringView next_arg = url_args.extractNextArg();
	if (not isDigit(next_arg) or request.method != server::HttpRequest::POST)
		return api_error400();

	submission_id = next_arg;

	InplaceBuff<30> sowner, c_owner;
	uint cu_mode;
	auto stmt = mysql.prepare("SELECT s.owner, c.owner, cu.mode"
		" FROM submissions s"
		" LEFT JOIN rounds c ON c.id=s.contest_round_id"
		" LEFT JOIN contests_users cu ON cu.contest_id=s.contest_round_id"
			" AND cu.user_id=?"
		" WHERE s.id=?");
	stmt.bindAndExecute(users_user_id, submission_id);
	stmt.res_bind_all(sowner, c_owner, cu_mode);
	if (not stmt.next())
		return api_error404();

	submission_perms = submission_get_permissions(sowner,
		(stmt.isNull(2) ? StringView{} : c_owner),
		(stmt.isNull(3) ? CUM::IS_NULL : CUM(cu_mode)));

	next_arg = url_args.extractNextArg();
	if (next_arg == "source")
		return api_submission_source();
	else if (next_arg == "download")
		return api_submission_download();
	else if (next_arg == "rejudge")
		return api_submission_rejudge();
	else if (next_arg == "chtype")
		return api_submission_change_type();
	else if (next_arg == "delete")
		return api_submission_delete();
	else
		return api_error404();
}

void Sim::api_submission_source() {
	STACK_UNWINDING_MARK;
}

void Sim::api_submission_download() {
	STACK_UNWINDING_MARK;
}

void Sim::api_submission_rejudge() {
	STACK_UNWINDING_MARK;
}

void Sim::api_submission_change_type() {
	STACK_UNWINDING_MARK;
}

void Sim::api_submission_delete() {
	STACK_UNWINDING_MARK;
}
