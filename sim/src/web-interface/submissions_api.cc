#include "sim.h"

#include <sim/jobs.h>
#include <sim/submission.h>
#include <simlib/filesystem.h>
#include <simlib/process.h>

using MySQL::bind_arg;
using std::string;

void Sim::append_submission_status(SubmissionStatus status,
	bool show_full_results)
{
	STACK_UNWINDING_MARK;
	using SS = SubmissionStatus;

	if (is_special(status)) {
		if (status == SS::PENDING)
			append("[\"\",\"Pending\"]");
		else if (status == SS::COMPILATION_ERROR)
			append("[\"purple\",\"Compilation failed\"]");
		else if (status == SS::CHECKER_COMPILATION_ERROR)
			append("[\"blue\",\"Checker compilation failed\"]");
		else if (status == SS::JUDGE_ERROR)
			append("[\"blue\",\"Judge error\"]");
		else
			append("[\"\",\"Unknown\"]");

	// Final
	} else if (show_full_results) {
		if ((status & SS::FINAL_MASK) == SS::OK)
			append("[\"green\",\"OK\"]");
		else if ((status & SS::FINAL_MASK) == SS::WA)
			append("[\"red\",\"Wrong answer\"]");
		else if ((status & SS::FINAL_MASK) == SS::TLE)
			append("[\"yellow\",\"Time limit exceeded\"]");
		else if ((status & SS::FINAL_MASK) == SS::MLE)
			append("[\"yellow\",\"Memory limit exceeded\"]");
		else if ((status & SS::FINAL_MASK) == SS::RTE)
			append("[\"intense-red\",\"Runtime error\"]");
		else
			append("[\"\",\"Unknown\"]");

	// Initial
	} else {
		if ((status & SS::INITIAL_MASK) == SS::INITIAL_OK)
			append("[\"initial green\",\"OK\"]");
		else if ((status & SS::INITIAL_MASK) == SS::INITIAL_WA)
			append("[\"initial red\",\"Wrong answer\"]");
		else if ((status & SS::INITIAL_MASK) == SS::INITIAL_TLE)
			append("[\"initial yellow\",\"Time limit exceeded\"]");
		else if ((status & SS::INITIAL_MASK) == SS::INITIAL_MLE)
			append("[\"initial yellow\",\"Memory limit exceeded\"]")
				;
		else if ((status & SS::INITIAL_MASK) == SS::INITIAL_RTE)
			append("[\"initial intense-red\",\"Runtime error\"]");
		else
			append("[\"\",\"Unknown\"]");
	}
}

#include <sys/time.h>

void Sim::api_submissions() {
	STACK_UNWINDING_MARK;
	using PERM = SubmissionPermissions;
	using CUM = ContestUserMode;

	if (not session_open())
		return api_error403();

	InplaceBuff<512> qfields, qwhere;
	InplaceBuff<60> qwhere_id_cond;
	qfields.append("SELECT s.id, s.type, s.language, s.owner, cu.mode, p.owner,"
		" u.username, s.problem_id, p.name, s.contest_problem_id, cp.name,"
		" cp.final_selecting_method, cp.reveal_score, s.contest_round_id,"
		" r.name, r.full_results, r.ends, s.contest_id, c.name, s.submit_time,"
		" s.status, s.score");
	qwhere.append(" FROM submissions s"
		" LEFT JOIN users u ON u.id=s.owner"
		" STRAIGHT_JOIN problems p ON p.id=s.problem_id"
		" LEFT JOIN contest_problems cp ON cp.id=s.contest_problem_id"
		" LEFT JOIN contest_rounds r ON r.id=s.contest_round_id"
		" LEFT JOIN contests c ON c.id=s.contest_id"
		" LEFT JOIN contest_users cu ON cu.contest_id=s.contest_id AND"
			" cu.user_id=", session_user_id,
		" WHERE s.type!=" STYPE_VOID_STR);

	enum ColumnIdx {
		SID, STYPE, SLANGUAGE, SOWNER, CUMODE, POWNER, SOWN_USERNAME, PROB_ID,
		PNAME, CPID, CP_NAME, CP_FSM, REVEAL_SCORE, CRID, CR_NAME, FULL_RES,
		CRENDS, CID, CNAME, SUBMIT_TIME, STATUS, SCORE, SOWN_FNAME, SOWN_LNAME,
		INIT_REPORT, FINAL_REPORT
	};

	bool allow_access = (session_user_type == UserType::ADMIN);
	bool select_one = false;
	bool only_final_or_only_normal = false;

	// Process restrictions
	StringView next_arg = url_args.extractNextArg();
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
			if (arg_id == "N") {
				qwhere.append(" AND s.type=" STYPE_NORMAL_STR);
				only_final_or_only_normal = true;
			} else if (arg_id == "F") {
				qwhere.append(" AND s.type=" STYPE_FINAL_STR);
				only_final_or_only_normal = true;
			} else if (arg_id == "I")
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

		// conditional (condition > is not supported because it creates problems
		// with protecting the final submissions from leak - it is solvable but
		// it is not easy and not worth it because such queries are not in use
		// now)
		if (cond == '<' and ~mask & ID_COND) {
			qwhere_id_cond.append(" AND s.id", arg);
			mask |= ID_COND;

		} else if (cond == '=' and ~mask & ID_COND) {
			allow_access = true; // Permissions will be checked while fetching
			                     // the data

			qfields.append(", u.first_name, u.last_name,"
				" s.initial_report, s.final_report");
			select_one = true;

			qwhere_id_cond.append(" AND s.id", arg);
			mask |= ID_COND;

		// problem's or round's id
		} else if (isIn(cond, {'p', 'C', 'R', 'P'}) and
			~mask & ROUND_OR_PROBLEM_ID_COND)
		{
			if (cond == 'p') {
				qwhere.append(" AND s.problem_id=", arg_id);
				if (not allow_access) {
					InplaceBuff<32> p_owner;
					auto stmt = mysql.prepare("SELECT owner FROM problems"
						" WHERE id=?");
					stmt.bindAndExecute(arg_id);
					stmt.res_bind_all(p_owner);
					if (stmt.next() and p_owner == session_user_id)
						allow_access = true;
				}

			} else {
				if (not allow_access) {
					StringView query;
					switch (cond) {
					case 'C': query = "SELECT cu.mode FROM contests c"
							" LEFT JOIN contest_users cu ON cu.user_id=?"
								" AND cu.contest_id=c.id"
							" WHERE c.id=?";
						break;

					case 'R': query = "SELECT cu.mode"
						" FROM contest_rounds r"
						" LEFT JOIN contest_users cu"
							" ON cu.contest_id=r.contest_id AND cu.user_id=?"
						" WHERE r.id=?";
						break;

					case 'P': query = "SELECT cu.mode"
						" FROM contest_problems p"
						" LEFT JOIN contest_users cu"
							" ON cu.contest_id=p.contest_id AND cu.user_id=?"
						" WHERE p.id=?";
						break;
					}
					auto stmt = mysql.prepare(query);
					stmt.bindAndExecute(session_user_id, arg_id);

					std::underlying_type_t<CUM> cu_mode;
					my_bool cu_mode_is_null;
					stmt.res_bind_all(bind_arg(cu_mode, cu_mode_is_null));
					if (not stmt.next())
						return api_error404(concat("Invalid query condition ",
							cond, " - no such round was found"));

					if (not cu_mode_is_null and
						isIn(CUM(cu_mode), {CUM::MODERATOR, CUM::OWNER}))
					{
						allow_access = true;
					}
				}

				if (cond == 'C')
					qwhere.append(" AND s.contest_id=", arg_id);
				else if (cond == 'R')
					qwhere.append(" AND s.contest_round_id=", arg_id);
				else if (cond == 'P')
					qwhere.append(" AND s.contest_problem_id=", arg_id);
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
	qfields.append(qwhere);
	auto res = mysql.query(concat(qfields, qwhere_id_cond,
		" ORDER BY s.id DESC LIMIT 50"));

	// Column names
	append("[\n{\"columns\":["
		"\"id\","
		"\"type\","
		"\"language\","
		"\"owner_id\","
		"\"owner_username\","
		"\"problem_id\","
		"\"problem_name\","
		"\"contest_problem_id\","
		"\"contest_problem_name\","
		"\"contest_round_id\","
		"\"contest_round_name\","
		"\"contest_id\","
		"\"contest_name\","
		"\"submit_time\","
		"{\"name\":\"status\",\"fields\":[\"class\",\"text\"]},"
		"\"score\","
		"\"actions\"");

	if (select_one)
		append(",\"owner_first_name\","
			"\"owner_last_name\","
			"\"initial_report\","
			"\"final_report\","
			"\"full_results\"");

	append("]}");

	auto curr_date = mysql_date();
	InplaceBuff<30> boundary_id;
	for (int appended_rows = 0; appended_rows < 50; ) {
		if (not res.next()) {
			if (select_one or res.rows_num() < 50)
				break;

			// Run another query to fetch more data
			// We are assuming here that the there is no condition on id or it
			// is '<'
			qwhere_id_cond.clear();
			qwhere_id_cond.append(" AND s.id<", boundary_id);
			// Execute query
			res = mysql.query(concat(qfields, qwhere_id_cond,
				" ORDER BY s.id DESC LIMIT 50"));
			continue;
		}

		boundary_id = res[SID];

		SubmissionType stype = SubmissionType(strtoull(res[STYPE]));
		SubmissionPermissions perms = submissions_get_permissions(res[SOWNER],
			stype,
			(res.is_null(CUMODE) ? CUM::IS_NULL : CUM(strtoull(res[CUMODE]))),
			res[POWNER]);

		if (perms == PERM::NONE)
			return api_error403();

		bool show_full_results = (bool(uint(perms & PERM::VIEW_FINAL_REPORT)) or
			res.is_null(FULL_RES) or res[FULL_RES] <= curr_date);
		bool hide_final_type = (not show_full_results and
			res[CP_FSM] == SFSM_WITH_HIGHEST_SCORE);

		// Hide (skip) submission as it would leak the final type and allow
		// figuring out the final status of a submission through prepared next
		// submissions
		if (hide_final_type and only_final_or_only_normal)
			continue;

		++appended_rows;

		// Submission id
		append(",\n[", res[SID], ',');

		// Type
		switch (stype) {
		case SubmissionType::NORMAL:
			append(hide_final_type ? "\"Normal / Final\"," : "\"Normal\",");
			break;
		case SubmissionType::FINAL:
			append(hide_final_type ? "\"Normal / Final\"," : "\"Final\",");
			break;
		case SubmissionType::IGNORED: append("\"Ignored\","); break;
		case SubmissionType::PROBLEM_SOLUTION: append("\"Problem solution\",");
			break;
		case SubmissionType::VOID: throw_assert(false); break;
		}

		append('"', toString(SubmissionLanguage(SubmissionLanguage(strtoull(
			res[SLANGUAGE])))), "\",");

		// Onwer's id
		if (res.is_null(SOWNER))
			append("null,");
		else
			append(res[SOWNER], ',');

		// Onwer's username
		if (res.is_null(SOWN_USERNAME))
			append("null,");
		else
			append("\"", res[SOWN_USERNAME], "\",");

		// Problem (id, name)
		if (res.is_null(PNAME))
			append("null,null,");
		else
			append(res[PROB_ID], ',', jsonStringify(res[PNAME]), ',');

		// Contest problem (id, name)
		if (res.is_null(CP_NAME))
			append("null,null,");
		else
			append(res[CPID], ',', jsonStringify(res[CP_NAME]), ',');

		// Contest round (id, name)
		if (res.is_null(CR_NAME))
			append("null,null,");
		else
			append(res[CRID], ',', jsonStringify(res[CR_NAME]), ',');

		// Contest (id, name)
		if (res.is_null(CNAME))
			append("null,null,");
		else
			append(res[CID], ',', jsonStringify(res[CNAME]), ',');

		// Submit time
		append("\"", res[SUBMIT_TIME], "\",");

		// Status: (CSS class, text)
		append_submission_status(SubmissionStatus(strtoull(res[STATUS])),
			show_full_results);

		// Score
		bool reveal_score = strtoull(res[REVEAL_SCORE]);
		if (not res.is_null(SCORE) and (show_full_results or reveal_score))
			append(',', res[SCORE], ',');
		else
			append(",null,");

		// Actions
		append('"');
		if (uint(perms & PERM::VIEW))
			append('v');
		if (uint(perms & PERM::VIEW_SOURCE))
			append('s');
		if (uint(perms & PERM::VIEW_RELATED_JOBS))
			append('j');
		if (uint(perms & PERM::VIEW) and
			(res.is_null(CRENDS) or curr_date < res[CRENDS])) // Round has not ended
		{
			// TODO: implement it in some way in the UI
			append('r'); // Resubmit solution
		}
		if (uint(perms & PERM::CHANGE_TYPE))
			append('C');
		if (uint(perms & PERM::REJUDGE))
			append('R');
		if (uint(perms & PERM::DELETE))
			append('D');

		append("\"");

		// Append
		if (select_one) {
			// User first and last name
			if (res.is_null(SOWN_FNAME))
				append(",null,null");
			else
				append(',', jsonStringify(res[SOWN_FNAME]), ',',
					jsonStringify(res[SOWN_LNAME]));

			// Reports (and round full results time if full report isn't shown)
			append(',', jsonStringify(res[INIT_REPORT]));
			if (show_full_results)
				append(',', jsonStringify(res[FINAL_REPORT]));
			else
				append(",null,\"", res[FULL_RES], '"');
		}

		append(']');
	}

	append("\n]");
}

void Sim::api_submission() {
	STACK_UNWINDING_MARK;
	using CUM = ContestUserMode;

	if (not session_open())
		return api_error403();

	StringView next_arg = url_args.extractNextArg();
	if (next_arg == "add")
		return api_submission_add();
	else if (not isDigit(next_arg))
		return api_error400();

	submissions_sid = next_arg;

	InplaceBuff<32> sowner, p_owner;
	std::underlying_type_t<SubmissionType> stype;
	std::underlying_type_t<SubmissionLanguage> slang;
	std::underlying_type_t<CUM> cu_mode;
	my_bool cu_mode_is_null;
	auto stmt = mysql.prepare("SELECT s.owner, s.type, s.language, cu.mode,"
			" p.owner"
		" FROM submissions s"
		" STRAIGHT_JOIN problems p ON p.id=s.problem_id"
		" LEFT JOIN contest_users cu ON cu.contest_id=s.contest_id"
			" AND cu.user_id=?"
		" WHERE s.id=?");
	stmt.bindAndExecute(session_user_id, submissions_sid);
	stmt.res_bind_all(sowner, stype, slang, bind_arg(cu_mode, cu_mode_is_null),
		p_owner);
	if (not stmt.next())
		return api_error404();

	submissions_slang = SubmissionLanguage(slang);
	submissions_perms = submissions_get_permissions(sowner,
		SubmissionType(stype), (cu_mode_is_null ? CUM::IS_NULL : CUM(cu_mode)),
		p_owner);

	// Subpages
	next_arg = url_args.extractNextArg();
	if (next_arg == "source")
		return api_submission_source();
	if (next_arg == "download")
		return api_submission_download();

	if (request.method != server::HttpRequest::POST)
		return api_error400();

	// Subpages causing action
	if (next_arg == "rejudge")
		return api_submission_rejudge();
	if (next_arg == "chtype")
		return api_submission_change_type();
	if (next_arg == "delete")
		return api_submission_delete();

	return api_error404();
}

void Sim::api_submission_add() {
	STACK_UNWINDING_MARK;

	if (not session_open())
		return api_error403();

	StringView next_arg = url_args.extractNextArg();
	// Load problem id and contest problem id (if specified)
	StringView problem_id, contest_problem_id;
	for (; next_arg.size(); next_arg = url_args.extractNextArg()) {
		if (next_arg[0] == 'p' and isDigit(next_arg.substr(1)) and
			problem_id.empty())
		{
			problem_id = next_arg.substr(1);
		} else if (hasPrefix(next_arg, "cp") and isDigit(next_arg.substr(2)) and
			contest_problem_id.empty())
		{
			contest_problem_id = next_arg.substr(2);
		} else
			return api_error400();
	}

	if (problem_id.empty())
		return api_error400("problem have to be specified");

	bool may_submit_ignored = false;
	InplaceBuff<32> contest_id, contest_round_id;
	// Problem submission
	if (contest_problem_id.empty()) {
		// Check permissions to the problem
		auto stmt = mysql.prepare("SELECT owner, type FROM problems WHERE id=?");
		InplaceBuff<32> powner;
		std::underlying_type_t<ProblemType> ptype;
		stmt.bindAndExecute(problem_id);
		stmt.res_bind_all(powner, ptype);
		if (not stmt.next())
			return api_error404();

		ProblemPermissions pperms = problems_get_permissions(powner,
			ProblemType(ptype));
		if (uint(~pperms & ProblemPermissions::SUBMIT))
			return api_error403();

		may_submit_ignored = uint(pperms & ProblemPermissions::SUBMIT_IGNORED);

	// Contest submission
	} else {
		using CUM = ContestUserMode;
		// Check permissions to the contest
		auto stmt = mysql.prepare("SELECT c.id, c.is_public, cr.id, cr.begins,"
				" cr.ends, cu.mode"
			" FROM contest_problems cp"
			" STRAIGHT_JOIN contest_rounds cr ON cr.id=cp.contest_round_id"
			" STRAIGHT_JOIN contests c ON c.id=cp.contest_id"
			" LEFT JOIN contest_users cu ON cu.contest_id=c.id AND cu.user_id=?"
			" WHERE cp.id=? AND cp.problem_id=?");
		stmt.bindAndExecute(session_user_id, contest_problem_id, problem_id);

		bool is_public;
		InplaceBuff<20> cr_begins, cr_ends;
		std::underlying_type_t<CUM> umode_u;
		my_bool umode_is_null, cr_begins_is_null, cr_ends_is_null;
		stmt.res_bind_all(contest_id, is_public, contest_round_id,
			bind_arg(cr_begins, cr_begins_is_null),
			bind_arg(cr_ends, cr_ends_is_null),
			bind_arg(umode_u, umode_is_null));
		if (not stmt.next())
			return api_error404();

		auto cperms = contests_get_permissions(is_public,
			(umode_is_null ? CUM::IS_NULL : CUM(umode_u)));

		if (uint(~cperms & ContestPermissions::PARTICIPATE))
			return api_error403(); // Could not participate

		auto curr_date = mysql_date();
		if (uint(~cperms & ContestPermissions::ADMIN) and
			((not cr_begins_is_null and curr_date < cr_begins)
				or (not cr_ends_is_null and cr_ends <= curr_date)))
		{
			return api_error403(); // Round has not begun jet or already ended
		}

		may_submit_ignored = uint(cperms & ContestPermissions::ADMIN);
	}

	// Validate fields
	SubmissionLanguage slang;
	auto slang_str = request.form_data.get("language");
	if (slang_str == "c")
		slang = SubmissionLanguage::C;
	else if (slang_str == "cpp")
		slang = SubmissionLanguage::CPP;
	else if (slang_str == "pascal")
		slang = SubmissionLanguage::PASCAL;
	else
		add_notification("error", "Invalid language");

	CStringView solution_tmp_path = request.form_data.file_path("solution");
	StringView code = request.form_data.get("code");
	bool ignored_submission = (may_submit_ignored and
		request.form_data.exist("ignored"));

	if ((code.empty() ^ request.form_data.get("solution").empty()) == 0) {
		add_notification("error", "You have to either choose a file or paste"
			" the code");
		return api_error400(notifications);
	}

	// Check the solution size
	// File
	if (code.empty()) {
		struct stat sb;
		if (stat(solution_tmp_path.c_str(), &sb))
			THROW("stat() failed", errmsg());

		// Check if solution is too big
		if ((uint64_t)sb.st_size > SOLUTION_MAX_SIZE) {
			add_notification("error", "Solution is too big (maximum allowed "
				"size: ", SOLUTION_MAX_SIZE, " bytes = ",
				SOLUTION_MAX_SIZE >> 10, " KB)");
			return api_error400(notifications);
		}
	// Code
	} else if (code.size() > SOLUTION_MAX_SIZE) {
		add_notification("error", "Solution is too big (maximum allowed "
			"size: ", SOLUTION_MAX_SIZE, " bytes = ",
			SOLUTION_MAX_SIZE >> 10, " KB)");
		return api_error400(notifications);
	}

	// Insert submission
	auto stmt = mysql.prepare("INSERT submissions (owner, problem_id,"
			" contest_problem_id, contest_round_id, contest_id, type, language,"
			" status, submit_time, last_judgment, initial_report, final_report)"
		" VALUES(?, ?, ?, ?, ?, " STYPE_VOID_STR ", ?, " SSTATUS_PENDING_STR ","
			" ?, ?, '', '')");
	if (contest_problem_id.empty())
		stmt.bindAndExecute(session_user_id, problem_id, nullptr, nullptr,
			nullptr, std::underlying_type_t<SubmissionLanguage>(slang),
			mysql_date(), mysql_date(0));
	else
		stmt.bindAndExecute(session_user_id, problem_id, contest_problem_id,
			contest_round_id, contest_id,
			std::underlying_type_t<SubmissionLanguage>(slang), mysql_date(),
			mysql_date(0));

	auto submission_id = stmt.insert_id();

	// Fill the submission's source file
	// File
	auto solution_dest = concat<64>("solutions/", submission_id);
	if (code.empty()) {
		if (rename(solution_tmp_path, solution_dest.to_cstr()))
			THROW("rename() failed", errmsg());
	// Code
	} else
		putFileContents(solution_dest.to_cstr(), code);

	// Create a job to judge the submission
	stmt = mysql.prepare("INSERT jobs (creator, status, priority, type, added,"
			" aux_id, info, data)"
		" VALUES(?, " JSTATUS_PENDING_STR ", ?, ?, ?, ?, ?, '')");
	stmt.bindAndExecute(session_user_id, priority(JobType::JUDGE_SUBMISSION),
		std::underlying_type_t<JobType>(JobType::JUDGE_SUBMISSION),
		mysql_date(), submission_id, jobs::dumpString(problem_id));

	// Activate the submission
	stmt = mysql.prepare("UPDATE submissions SET type=? WHERE id=?");
	stmt.bindAndExecute(
		std::underlying_type_t<SubmissionType>(ignored_submission ?
			SubmissionType::IGNORED : SubmissionType::NORMAL),
		submission_id);

	jobs::notify_job_server();
	append(submission_id);
}

void Sim::api_submission_source() {
	STACK_UNWINDING_MARK;

	if (uint(~submissions_perms & SubmissionPermissions::VIEW_SOURCE))
		return api_error403();

	append(cpp_syntax_highlighter(getFileContents(
		concat("solutions/", submissions_sid).to_cstr())));
}

void Sim::api_submission_download() {
	STACK_UNWINDING_MARK;

	if (uint(~submissions_perms & SubmissionPermissions::VIEW_SOURCE))
		return api_error403();


	resp.headers["Content-type"] = to_MIME(submissions_slang);
	resp.headers["Content-Disposition"] = concat_tostr("attachment; filename=",
		submissions_sid, to_extension(submissions_slang));

	resp.content = concat("solutions/", submissions_sid);
	resp.content_type = server::HttpResponse::FILE;
}

void Sim::api_submission_rejudge() {
	STACK_UNWINDING_MARK;

	if (uint(~submissions_perms & SubmissionPermissions::REJUDGE))
		return api_error403();

	InplaceBuff<32> problem_id;
	auto stmt = mysql.prepare("SELECT problem_id FROM submissions WHERE id=?");
	stmt.bindAndExecute(submissions_sid);
	stmt.res_bind_all(problem_id);
	throw_assert(stmt.next());

	stmt = mysql.prepare("INSERT jobs (creator, status, priority, type, added,"
			" aux_id, info, data)"
		" VALUES(?, " JSTATUS_PENDING_STR ", ?, ?, ?, ?, ?, '')");
	stmt.bindAndExecute(session_user_id, priority(JobType::JUDGE_SUBMISSION),
		(uint)JobType::JUDGE_SUBMISSION, mysql_date(), submissions_sid,
		jobs::dumpString(problem_id));

	jobs::notify_job_server();
}

void Sim::api_submission_change_type() {
	STACK_UNWINDING_MARK;
	using SS = SubmissionStatus;
	using ST = SubmissionType;

	if (uint(~submissions_perms & SubmissionPermissions::CHANGE_TYPE))
		return api_error403();

	StringView type_s = request.form_data.get("type");

	SubmissionType new_type;
	if (type_s == "I")
		new_type = ST::IGNORED;
	else if (type_s == "N")
		new_type = ST::NORMAL;
	else
		return api_error400("Invalid type, it must be one of those: I or N");

	// Lock the table to be able to safely modify the submission
	// locking contest_problems is required by update_final()
	mysql.update("LOCK TABLES submissions WRITE, contest_problems READ");
	auto lock_guard = make_call_in_destructor([&]{
		mysql.update("UNLOCK TABLES");
	});

	auto res = mysql.query(concat("SELECT status, owner, contest_problem_id"
		" FROM submissions WHERE id=", submissions_sid));
	throw_assert(res.next());

	SS status = SS(strtoull(res[0]));
	StringView owner = (res.is_null(1) ? "" : res[1]);
	StringView contest_problem_id = (res.is_null(2) ? "" : res[2]);

	// Cannot be FINAL
	if (is_special(status)) {
		auto stmt = mysql.prepare("UPDATE submissions SET type=? WHERE id=?");
		stmt.bindAndExecute(uint(new_type), submissions_sid);
		return;
	}

	// May be of type FINAL
	SignalBlocker sb; // This part shouldn't be interrupted

	// Update the submission first
	auto stmt = mysql.prepare("UPDATE submissions SET type=?, final_candidate=?"
		" WHERE id=?");
	stmt.bindAndExecute(uint(new_type), (new_type == ST::NORMAL),
		submissions_sid);

	submission::update_final(mysql, owner, contest_problem_id, false);
}

void Sim::api_submission_delete() {
	STACK_UNWINDING_MARK;

	if (uint(~submissions_perms & SubmissionPermissions::DELETE))
		return api_error403();

	// Lock the table to be able to safely delete the submission
	// locking contest_problems is required by update_final()
	mysql.update("LOCK TABLES submissions WRITE, contest_problems READ");
	auto lock_guard = make_call_in_destructor([&]{
		mysql.update("UNLOCK TABLES");
	});

	auto res = mysql.query(concat("SELECT owner, contest_problem_id"
		" FROM submissions WHERE id=", submissions_sid));
	throw_assert(res.next());

	SignalBlocker sb; // This part shouldn't be interrupted

	// TODO: maybe make a job out of it

	mysql.update(concat("DELETE FROM submissions WHERE id=", submissions_sid));

	submission::update_final(mysql, (res.is_null(0) ? "" : res[0]),
		(res.is_null(1) ? "" : res[1]), false);
}
