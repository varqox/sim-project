#include "sim.h"

#include <sim/jobs.h>
#include <simlib/config_file.h>
#include <simlib/filesystem.h>
#include <simlib/sim/problem_package.h>
#include <simlib/zip.h>

static constexpr const char* proiblem_type_str(ProblemType type) noexcept {
	using PT = ProblemType;

	switch (type) {
	case PT::PUBLIC: return "Public";
	case PT::PRIVATE: return "Private";
	case PT::CONTEST_ONLY: return "Contest only";
	case PT::VOID: return "Void";
	}
	return "Unknown";
}

void Sim::api_problems() {
	STACK_UNWINDING_MARK;

	using PERM = ProblemPermissions;

	InplaceBuff<512> qfields, qwhere;
	qfields.append("SELECT p.id, p.added, p.type, p.name, p.label, p.owner,"
		" u.username");
	qwhere.append(" FROM problems p LEFT JOIN users u ON p.owner=u.id"
		" WHERE p.type!=" JTYPE_VOID_STR);

	enum ColumnIdx {
		PID, ADDED, PTYPE, NAME, LABEL, OWNER, OWN_USERNAME, SIMFILE
	};

	// Get the overall permissions to the problems set
	session_open();
	problems_perms = problems_get_permissions();
	// Choose problems to select
	if (not uint(problems_perms & PERM::ADMIN)) {
		if (not session_is_open) {
			throw_assert(uint(problems_perms & PERM::VIEW_TPUBLIC));
			qwhere.append(" AND p.type=" PTYPE_PUBLIC_STR);

		} else if (session_user_type == UserType::TEACHER) {
			throw_assert(uint(problems_perms & PERM::VIEW_TPUBLIC) and
				uint(problems_perms & PERM::VIEW_TCONTEST_ONLY));
			qwhere.append(" AND (p.type=" PTYPE_PUBLIC_STR " OR p.type="
				PTYPE_CONTEST_ONLY_STR " OR p.owner=", session_user_id, ')');

		} else {
			throw_assert(uint(problems_perms & PERM::VIEW_TPUBLIC));
			qwhere.append(" AND (p.type=" PTYPE_PUBLIC_STR " OR p.owner=",
				session_user_id, ')');
		}
	}

	// Process restrictions
	bool select_specified_problem = false;
	StringView next_arg = url_args.extractNextArg();
	for (uint mask = 0; next_arg.size(); next_arg = url_args.extractNextArg()) {
		constexpr uint ID_COND = 1;
		constexpr uint PTYPE_COND = 2;
		constexpr uint USER_ID_COND = 4;

		auto arg = decodeURI(next_arg);
		char cond = arg[0];
		StringView arg_id = StringView{arg}.substr(1);

		// problem type
		if (cond == 't' and ~mask & PTYPE_COND) {
			if (arg_id == "PUB")
				qwhere.append(" AND p.type=" PTYPE_PUBLIC_STR);
			else if (arg_id == "PRI")
				qwhere.append(" AND p.type=" PTYPE_PRIVATE_STR);
			else if (arg_id == "CON")
				qwhere.append(" AND p.type=" PTYPE_CONTEST_ONLY_STR);
			else
				return api_error400(concat("Invalid problem type: ", arg_id));

			mask |= PTYPE_COND;
			continue;
		}

		if (not isDigit(arg_id))
			return api_error400();

		// conditional
		if (isIn(cond, {'<', '>'}) and ~mask & ID_COND) {
			qwhere.append(" AND p.id", arg);
			mask |= ID_COND;

		} else if (cond == '=' and ~mask & ID_COND) {
			qfields.append(", p.simfile");
			select_specified_problem = true;
			qwhere.append(" AND p.id", arg);
			mask |= ID_COND;

		// user (owner)
		} else if (cond == 'u' and ~mask & USER_ID_COND) {
			// Prevent bypassing not set VIEW_OWNER permission
			if (not session_is_open or (arg_id != session_user_id and
				not uint(problems_perms & PERM::SELECT_BY_OWNER)))
			{
				return api_error403("You have no permissions to select others'"
					" problems by their user id");
			}

			qwhere.append(" AND p.owner=", arg_id);
			mask |= USER_ID_COND;

		} else
			return api_error400();

	}

	// Execute query
	qfields.append(qwhere, " ORDER BY p.id DESC LIMIT 50");
	auto res = mysql.query(qfields);

	// Column names
	append("[\n{\"columns\":["
			"\"id\","
			"\"added\","
			"\"type\","
			"\"name\","
			"\"label\","
			"\"owner_id\","
			"\"owner_username\","
			"\"actions\","
			"{\"name\":\"tags\",\"fields\":["
				"\"public\","
				"\"hidden\""
			"]},"
			"\"simfile\","
			"\"memory_limit\""
		"]}");

	// Tags selector
	uint64_t pid;
	bool hidden;
	InplaceBuff<PROBLEM_TAG_MAX_LEN> tag;
	auto stmt = mysql.prepare("SELECT tag FROM problem_tags"
		" WHERE problem_id=? AND hidden=? ORDER BY tag");
	stmt.bind_all(pid, hidden);
	stmt.res_bind_all(tag);

	while (res.next()) {
		ProblemType problem_type {ProblemType(strtoull(res[PTYPE]))};
		auto perms = problems_get_permissions(res[OWNER], problem_type);

		// Id
		append(",\n[", res[PID], ",");

		// Added
		if (uint(perms & PERM::VIEW_ADD_TIME))
			append("\"", res[ADDED], "\",");
		else
			append("null,");

		// Type
		append("\"", proiblem_type_str(problem_type), "\",");
		// Name
		append(jsonStringify(res[NAME]), ',');
		// Label
		append(jsonStringify(res[LABEL]), ',');

		// Owner
		if (uint(perms & PERM::VIEW_OWNER)) {
			append(res[OWNER], ',');

			if (res.is_null(OWN_USERNAME))
				append("null,");
			else
				append("\"", res[OWN_USERNAME], "\",");
		} else
			append("null,null,");

		// Append what buttons to show
		append('"');
		if (uint(perms & PERM::VIEW))
			append('v');
		if (uint(perms & PERM::VIEW_STATEMENT))
			append('V');
		if (uint(perms & PERM::VIEW_TAGS))
			append('t');
		if (uint(perms & PERM::VIEW_HIDDEN_TAGS))
			append('h');
		if (uint(perms & PERM::VIEW_SOLUTIONS_AND_SUBMISSIONS))
			append('s');
		if (uint(perms & PERM::VIEW_SIMFILE))
			append('f');
		if (uint(perms & PERM::VIEW_OWNER))
			append('o');
		if (uint(perms & PERM::VIEW_ADD_TIME))
			append('a');
		if (uint(perms & PERM::VIEW_RELATED_JOBS))
			append('j');
		if (uint(perms & PERM::DOWNLOAD))
			append('d');
		if (uint(perms & PERM::SUBMIT))
			append('S');
		if (uint(perms & PERM::EDIT))
			append('E');
		if (uint(perms & PERM::SUBMIT_IGNORED))
			append('i');
		if (uint(perms & PERM::REUPLOAD))
			append('R');
		if (uint(perms & PERM::REJUDGE_ALL))
			append('J');
		if (uint(perms & PERM::EDIT_TAGS))
			append('T');
		if (uint(perms & PERM::EDIT_HIDDEN_TAGS))
			append('H');
		if (uint(perms & PERM::DELETE))
			append('D');
		append("\",");

		auto append_tags = [&](bool h) {
			pid = strtoull(res[PID]);
			hidden = h;
			stmt.execute();
			if (stmt.next())
				for (;;) {
					append(jsonStringify(tag));
					// Select next or break
					if (stmt.next())
						append(',');
					else
						break;
				}
		};

		// Tags
		append("[[");
		// Normal
		if (uint(perms & PERM::VIEW_TAGS))
			append_tags(false);
		append("],[");
		// Hidden
		if (uint(perms & PERM::VIEW_HIDDEN_TAGS))
			append_tags(true);
		append("]]");

		// Append simfile and memory limit
		if (select_specified_problem and uint(perms & PERM::VIEW_SIMFILE)) {
			ConfigFile cf;
			cf.addVars("memory_limit");
			cf.loadConfigFromString(res[SIMFILE].to_string());
			append(',', jsonStringify(res[SIMFILE]), // simfile
				',', jsonStringify(cf.getVar("memory_limit").asString()));
		}

		append(']');
	}

	append("\n]");
}

void Sim::api_problem() {
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
	std::underlying_type_t<ProblemType> ptype;

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
	else if (next_arg == "edit")
		return api_problem_edit();
	// else if (next_arg == "delete")
	// 	return api_problem_delete();
	else
		return api_error400();
}

void Sim::api_problem_add_or_reupload_impl(bool reuploading) {
	// Validate fields
	CStringView name, label, memory_limit, global_time_limit, package_file;
	bool relative_time_limits = request.form_data.exist("relative_time_limits");
	bool ignore_simfile = request.form_data.exist("ignore_simfile");
	// TODO: implement it (the line below)
	// bool seek_for_new_tests = request.form_data.exist("seek_for_new_tests");

	form_validate(name, "name", "Problem's name", PROBLEM_NAME_MAX_LEN);
	form_validate(label, "label", "Problem's label", PROBLEM_LABEL_MAX_LEN);
	form_validate(memory_limit, "mem_limit", "Memory limit",
		isDigitNotGreaterThan<(std::numeric_limits<uint64_t>::max() >> 20)>);
	form_validate_file_path_not_blank(package_file, "package",
		"Zipped package");
	form_validate(global_time_limit, "global_time_limit", "Global time limit",
		isReal); // TODO: add length limit
	// Convert global_time_limit to usec
	uint64_t gtl =
		round(strtod(global_time_limit.c_str(), nullptr) * 1'000'000);
	if (global_time_limit.size() && gtl < 400000)
		add_notification("error",
			"Global time limit cannot be lower than 0.4 s");

	// Validate problem type
	StringView ptype_str = request.form_data.get("type");
	ProblemType ptype /*= ProblemType::VOID*/;
	if (ptype_str == "PRI")
		ptype = ProblemType::PRIVATE;
	else if (ptype_str == "PUB")
		ptype = ProblemType::PUBLIC;
	else if (ptype_str == "CON")
		ptype = ProblemType::CONTEST_ONLY;
	else
		add_notification("error", "Invalid problem's type");

	if (form_validation_error)
		return api_error400(notifications);

	jobs::AddProblemInfo ap_info {
		name.to_string(),
		label.to_string(),
		strtoull(memory_limit.c_str()),
		gtl,
		relative_time_limits,
		ignore_simfile,
		ptype
	};

	auto stmt = mysql.prepare("INSERT jobs(creator, priority, type, status,"
			" added, aux_id, info, data)"
		" VALUES(?, ?, " JTYPE_VOID_STR ", " JSTATUS_PENDING_STR ", ?, ?, ?,"
			" '')");
	if (reuploading)
		stmt.bindAndExecute(session_user_id, priority(JobType::ADD_PROBLEM),
			mysql_date(), problems_pid, ap_info.dump());
	else
		stmt.bindAndExecute(session_user_id, priority(JobType::ADD_PROBLEM),
			mysql_date(), nullptr, ap_info.dump());

	auto job_id = stmt.insert_id();
	// Make the package file the job's file
	if (rename(package_file, concat("jobs_files/", job_id, ".zip").to_cstr()))
		THROW("move() failed", errmsg());

	// Activate the job
	stmt = mysql.prepare("UPDATE jobs SET type=? WHERE id=?");
	stmt.bindAndExecute(std::underlying_type_t<JobType>(reuploading	?
		JobType::REUPLOAD_PROBLEM : JobType::ADD_PROBLEM), job_id);

	jobs::notify_job_server();

	append(job_id);
}

void Sim::api_problem_add() {
	STACK_UNWINDING_MARK;
	using PERM = ProblemPermissions;

	if (uint(~problems_perms & PERM::ADD))
		return api_error403();

	api_problem_add_or_reupload_impl(false);
}

void Sim::api_statement_impl(StringView problem_id, StringView problem_label,
	StringView simfile)
{
	STACK_UNWINDING_MARK;

	ConfigFile cf;
	cf.addVars("statement");
	cf.loadConfigFromString(simfile.to_string());

	auto& statement = cf.getVar("statement").asString();
	StringView ext;
	if (hasSuffix(statement, ".pdf")) {
		ext = ".pdf";
		resp.headers["Content-type"] = "application/pdf";
	} else if (hasSuffixIn(statement, {".html", ".htm"})) {
		ext = ".html";
		resp.headers["Content-type"] = "text/html";
	} else if (hasSuffixIn(statement, {".txt", ".md"})) {
		ext = ".md";
		resp.headers["Content-type"] = "text/markdown; charset=utf-8";
	}

	resp.headers["Content-Disposition"] =
		concat_tostr("inline; filename=",
			http::quote(concat(problem_label, ext)));

	// TODO: add some cache system for the statements

	auto package_zip = concat("problems/", problem_id, ".zip");
	resp.content = extract_file_from_zip(package_zip.to_cstr(), concat(
		sim::zip_package_master_dir(package_zip.to_cstr()), statement)
			.to_cstr());
}

void Sim::api_problem_statement(StringView problem_label, StringView simfile) {
	STACK_UNWINDING_MARK;
	using PERM = ProblemPermissions;

	if (uint(~problems_perms & PERM::VIEW_STATEMENT))
		return api_error403();

	return api_statement_impl(problems_pid, problem_label, simfile);
}

void Sim::api_problem_download(StringView problem_label) {
	STACK_UNWINDING_MARK;
	using PERM = ProblemPermissions;

	if (uint(~problems_perms & PERM::DOWNLOAD))
		return api_error403();

	resp.headers["Content-Disposition"] =
		concat_tostr("attachment; filename=", problem_label, ".zip");
	resp.content_type = server::HttpResponse::FILE;
	resp.content = concat("problems/", problems_pid, ".zip");
}

void Sim::api_problem_rejudge_all_submissions() {
	STACK_UNWINDING_MARK;
	using PERM = ProblemPermissions;

	if (uint(~problems_perms & PERM::REJUDGE_ALL))
		return api_error403();

	auto stmt = mysql.prepare("INSERT jobs (creator, status, priority, type,"
			" added, aux_id, info, data)"
		" SELECT ?, " JSTATUS_PENDING_STR ", ?, " JTYPE_JUDGE_SUBMISSION_STR
			", ?, id, ?, ''"
		" FROM submissions WHERE problem_id=? ORDER BY id");
	stmt.bindAndExecute(session_user_id, priority(JobType::JUDGE_SUBMISSION),
		mysql_date(), jobs::dumpString(problems_pid), problems_pid);

	jobs::notify_job_server();
}

void Sim::api_problem_reupload() {
	STACK_UNWINDING_MARK;
	using PERM = ProblemPermissions;

	if (uint(~problems_perms & PERM::REUPLOAD))
		return api_error403();

	api_problem_add_or_reupload_impl(true);
}

void Sim::api_problem_edit() {
	STACK_UNWINDING_MARK;

	StringView next_arg = url_args.extractNextArg();
	if (next_arg == "tags")
		return api_problem_edit_tags();
	else
		return api_error400();
}

void Sim::api_problem_edit_tags() {
	STACK_UNWINDING_MARK;
	using PERM = ProblemPermissions;

	auto add_tag = [&] {
		bool hidden = (request.form_data.get("hidden") == "true");
		StringView name;
		form_validate_not_blank(name, "name", "Tag name", PROBLEM_TAG_MAX_LEN);
		if (form_validation_error)
			return api_error400(notifications);

		if (uint(~problems_perms & (hidden ? PERM::EDIT_HIDDEN_TAGS : PERM::EDIT_TAGS)))
			return api_error403();

		auto stmt = mysql.prepare("INSERT IGNORE"
			" INTO problem_tags(problem_id, tag, hidden) VALUES(?, ?, ?)");
		stmt.bindAndExecute(problems_pid, name, hidden);

		if (stmt.affected_rows() == 0)
			return api_error400("Tag already exist");
	};

	auto edit_tag = [&] {
		bool hidden = (request.form_data.get("hidden") == "true");
		StringView name, old_name;
		form_validate_not_blank(name, "name", "Tag name", PROBLEM_TAG_MAX_LEN);
		form_validate_not_blank(old_name, "old_name", "Old tag name",
			PROBLEM_TAG_MAX_LEN);
		if (form_validation_error)
			return api_error400(notifications);

		if (uint(~problems_perms & (hidden ? PERM::EDIT_HIDDEN_TAGS : PERM::EDIT_TAGS)))
			return api_error403();

		auto stmt = mysql.prepare("UPDATE IGNORE problem_tags SET tag=?"
			" WHERE problem_id=? AND tag=? AND hidden=?");
		stmt.bindAndExecute(name, problems_pid, old_name, hidden);

		if (stmt.affected_rows() == 0 and mysql_warning_count(mysql))
			return api_error400("Tag already exist");
	};

	auto delete_tag = [&] {
		StringView name;
		bool hidden = (request.form_data.get("hidden") == "true");
		form_validate_not_blank(name, "name", "Tag name", PROBLEM_TAG_MAX_LEN);
		if (form_validation_error)
			return api_error400(notifications);

		if (uint(~problems_perms & (hidden ? PERM::EDIT_HIDDEN_TAGS : PERM::EDIT_TAGS)))
			return api_error403();

		auto stmt = mysql.prepare("DELETE FROM problem_tags"
			" WHERE problem_id=? AND tag=? AND hidden=?");
		stmt.bindAndExecute(problems_pid, name, hidden);
	};

	StringView next_arg = url_args.extractNextArg();
	if (next_arg == "add_tag")
		return add_tag();
	if (next_arg == "edit_tag")
		return edit_tag();
	if (next_arg == "delete_tag")
		return delete_tag();
	else
		return api_error400();
}
