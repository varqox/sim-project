#include "sim.h"

#include <sim/jobs.h>
#include <simlib/config_file.h>
#include <simlib/filesystem.h>
#include <simlib/libzip.h>
#include <simlib/sim/problem_package.h>

static constexpr const char* proiblem_type_str(ProblemType type) noexcept {
	using PT = ProblemType;

	switch (type) {
	case PT::PUBLIC: return "Public";
	case PT::PRIVATE: return "Private";
	case PT::CONTEST_ONLY: return "Contest only";
	}
	return "Unknown";
}

void Sim::api_problems() {
	STACK_UNWINDING_MARK;

	using PERM = ProblemPermissions;

	// We may read data several times (permission checking), so transaction is
	// used to ensure data consistency
	auto transaction = mysql.start_transaction();

	InplaceBuff<512> qfields, qwhere;
	qfields.append("SELECT p.id, p.added, p.type, p.name, p.label, p.owner, "
	               "u.username, s.full_status");
	qwhere.append(" FROM problems p LEFT JOIN users u ON p.owner=u.id "
	              "LEFT JOIN submissions s ON s.owner=",
	              (session_is_open ? StringView(session_user_id) : "''"),
	              " AND s.problem_id=p.id AND s.problem_final=1 "
	              "WHERE TRUE"); // Needed to easily append constraints

	enum ColumnIdx {
		PID,
		ADDED,
		PTYPE,
		NAME,
		LABEL,
		OWNER,
		OWN_USERNAME,
		SFULL_STATUS,
		SIMFILE
	};

	problems_perms = problems_get_overall_permissions();
	// Choose problems to select
	if (not uint(problems_perms & PERM::VIEW_ALL)) {
		if (not session_is_open) {
			throw_assert(uint(problems_perms & PERM::VIEW_TPUBLIC));
			qwhere.append(" AND p.type=" PTYPE_PUBLIC_STR);

		} else if (session_user_type == UserType::TEACHER) {
			throw_assert(uint(problems_perms & PERM::VIEW_TPUBLIC) and
			             uint(problems_perms & PERM::VIEW_TCONTEST_ONLY));
			qwhere.append(" AND (p.type=" PTYPE_PUBLIC_STR
			              " OR p.type=" PTYPE_CONTEST_ONLY_STR " OR p.owner=",
			              session_user_id, ')');

		} else {
			throw_assert(uint(problems_perms & PERM::VIEW_TPUBLIC));
			qwhere.append(" AND (p.type=" PTYPE_PUBLIC_STR " OR p.owner=",
			              session_user_id, ')');
		}
	}

	// Process restrictions
	auto rows_limit = API_FIRST_QUERY_ROWS_LIMIT;
	bool select_specified_problem = false;
	StringView next_arg = url_args.extractNextArg();
	for (uint mask = 0; next_arg.size(); next_arg = url_args.extractNextArg()) {
		constexpr uint ID_COND = 1;
		constexpr uint PTYPE_COND = 2;
		constexpr uint USER_ID_COND = 4;

		auto arg = decodeURI(next_arg);
		char cond = arg[0];
		StringView arg_id = StringView(arg).substr(1);

		// problem type
		if (cond == 't' and ~mask & PTYPE_COND) {
			if (arg_id == "PUB")
				qwhere.append(" AND p.type=" PTYPE_PUBLIC_STR);
			else if (arg_id == "PRI")
				qwhere.append(" AND p.type=" PTYPE_PRIVATE_STR);
			else if (arg_id == "CON")
				qwhere.append(" AND p.type=" PTYPE_CONTEST_ONLY_STR);
			else
				return api_error400(intentionalUnsafeStringView(
				   concat("Invalid problem type: ", arg_id)));

			mask |= PTYPE_COND;

		} else if (not isDigit(arg_id)) {
			return api_error400();

		} else if (isOneOf(cond, '<', '>') and ~mask & ID_COND) { // Conditional
			rows_limit = API_OTHER_QUERY_ROWS_LIMIT;
			qwhere.append(" AND p.id", arg);
			mask |= ID_COND;

		} else if (cond == '=' and ~mask & ID_COND) {
			qfields.append(", p.simfile");
			select_specified_problem = true;
			qwhere.append(" AND p.id", arg);
			mask |= ID_COND;

		} else if (cond == 'u' and ~mask & USER_ID_COND) { // User (owner)
			// Prevent bypassing unset VIEW_OWNER permission
			if (not session_is_open or
			    (arg_id != session_user_id and
			     not uint(problems_perms & PERM::SELECT_BY_OWNER))) {
				return api_error403("You have no permissions to select others' "
				                    "problems by their user id");
			}

			qwhere.append(" AND p.owner=", arg_id);
			mask |= USER_ID_COND;

		} else {
			return api_error400();
		}
	}

	// Execute query
	qfields.append(qwhere, " ORDER BY p.id DESC LIMIT ", rows_limit);
	auto res = mysql.query(qfields);

	// Column names
	// clang-format off
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
	           "\"color_class\","
	           "\"simfile\","
	           "\"memory_limit\""
	       "]}");
	// clang-format on

	// Tags selector
	uint64_t pid;
	bool hidden;
	InplaceBuff<PROBLEM_TAG_MAX_LEN> tag;
	auto stmt = mysql.prepare("SELECT tag FROM problem_tags "
	                          "WHERE problem_id=? AND hidden=? ORDER BY tag");
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
		if (uint(perms & PERM::VIEW_OWNER) and not res.is_null(OWNER))
			append(res[OWNER], ",\"", res[OWN_USERNAME], "\",");
		else
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
			append('L');
		if (uint(perms & PERM::RESET_TIME_LIMITS))
			append('J');
		if (uint(perms & PERM::EDIT_TAGS))
			append('T');
		if (uint(perms & PERM::EDIT_HIDDEN_TAGS))
			append('H');
		if (uint(perms & PERM::CHANGE_STATEMENT))
			append('C');
		if (uint(perms & PERM::DELETE))
			append('D');
		if (uint(perms & PERM::MERGE))
			append('M');
		if (uint(perms & PERM::VIEW_ATTACHING_CONTEST_PROBLEMS))
			append('c');
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

		// Append color class
		if (res.is_null(SFULL_STATUS)) {
			append(",null");
		} else {
			append(
			   ",\"",
			   css_color_class(SubmissionStatus(strtoull(res[SFULL_STATUS]))),
			   "\"");
		}

		// Append simfile and memory limit
		if (select_specified_problem and uint(perms & PERM::VIEW_SIMFILE)) {
			ConfigFile cf;
			cf.add_vars("memory_limit");
			cf.load_config_from_string(res[SIMFILE].to_string());
			append(',', jsonStringify(res[SIMFILE]), // simfile
			       ',', jsonStringify(cf.get_var("memory_limit").as_string()));
		}

		append(']');
	}

	append("\n]");
}

void Sim::api_problem() {
	STACK_UNWINDING_MARK;

	problems_perms = problems_get_overall_permissions();

	StringView next_arg = url_args.extractNextArg();
	if (next_arg == "add")
		return api_problem_add();
	else if (not isDigit(next_arg))
		return api_error400();

	problems_pid = next_arg;

	InplaceBuff<32> powner;
	InplaceBuff<PROBLEM_LABEL_MAX_LEN> plabel;
	InplaceBuff<1 << 16> simfile;
	EnumVal<ProblemType> ptype;

	auto stmt = mysql.prepare("SELECT file_id, owner, type, label, simfile "
	                          "FROM problems WHERE id=?");
	stmt.bindAndExecute(problems_pid);
	stmt.res_bind_all(problems_file_id, powner, ptype, plabel, simfile);
	if (not stmt.next())
		return api_error404();

	problems_perms = problems_get_permissions(powner, ptype);

	next_arg = url_args.extractNextArg();
	if (next_arg == "statement")
		return api_problem_statement(plabel, simfile);
	else if (next_arg == "download")
		return api_problem_download(plabel);
	else if (next_arg == "rejudge_all_submissions")
		return api_problem_rejudge_all_submissions();
	else if (next_arg == "reset_time_limits")
		return api_problem_reset_time_limits();
	else if (next_arg == "reupload")
		return api_problem_reupload();
	else if (next_arg == "edit")
		return api_problem_edit();
	else if (next_arg == "change_statement")
		return api_problem_change_statement();
	else if (next_arg == "delete")
		return api_problem_delete();
	else if (next_arg == "merge_into_another")
		return api_problem_merge_into_another();
	else if (next_arg == "attaching_contest_problems")
		return api_problem_attaching_contest_problems();
	else
		return api_error400();
}

void Sim::api_problem_add_or_reupload_impl(bool reuploading) {
	STACK_UNWINDING_MARK;

	// Validate fields
	CStringView name, label, memory_limit, global_time_limit, package_file;
	bool reset_time_limits = request.form_data.exist("reset_time_limits");
	bool ignore_simfile = request.form_data.exist("ignore_simfile");
	bool seek_for_new_tests = request.form_data.exist("seek_for_new_tests");
	bool reset_scoring = request.form_data.exist("reset_scoring");

	form_validate(name, "name", "Problem's name", PROBLEM_NAME_MAX_LEN);
	form_validate(label, "label", "Problem's label", PROBLEM_LABEL_MAX_LEN);
	form_validate(
	   memory_limit, "mem_limit", "Memory limit",
	   isDigitNotGreaterThan<(std::numeric_limits<uint64_t>::max() >> 20)>);
	form_validate_file_path_not_blank(package_file, "package",
	                                  "Zipped package");
	form_validate(global_time_limit, "global_time_limit", "Global time limit",
	              isReal); // TODO: add length limit

	using std::chrono::duration;
	using std::chrono::duration_cast;
	using std::chrono::nanoseconds;

	// Convert global_time_limit
	decltype(jobs::AddProblemInfo::global_time_limit) gtl;
	if (not global_time_limit.empty()) {
		gtl = duration_cast<nanoseconds>(
		   duration<double>(strtod(global_time_limit.c_str(), nullptr)));
		if (gtl.value() < MIN_TIME_LIMIT) {
			add_notification("error", "Global time limit cannot be lower than ",
			                 toString(MIN_TIME_LIMIT), " s");
		}
	}

	// Memory limit
	decltype(jobs::AddProblemInfo::memory_limit) mem_limit;
	if (not memory_limit.empty())
		mem_limit = strtoull(memory_limit.c_str());

	// Validate problem type
	StringView ptype_str = request.form_data.get("type");
	ProblemType ptype;
	if (ptype_str == "PRI")
		ptype = ProblemType::PRIVATE;
	else if (ptype_str == "PUB")
		ptype = ProblemType::PUBLIC;
	else if (ptype_str == "CON")
		ptype = ProblemType::CONTEST_ONLY;
	else
		add_notification("error", "Invalid problem's type");

	if (notifications.size)
		return api_error400(notifications);

	jobs::AddProblemInfo ap_info {name.to_string(),
	                              label.to_string(),
	                              mem_limit,
	                              gtl,
	                              reset_time_limits,
	                              ignore_simfile,
	                              seek_for_new_tests,
	                              reset_scoring,
	                              ptype};

	auto transaction = mysql.start_transaction();
	mysql.update("INSERT INTO internal_files VALUES()");
	auto job_file_id = mysql.insert_id();
	FileRemover job_file_remover(internal_file_path(job_file_id));

	// Make the uploaded package file the job's file
	if (move(package_file, internal_file_path(job_file_id)))
		THROW("move()", errmsg());

	EnumVal<JobType> jtype =
	   (reuploading ? JobType::REUPLOAD_PROBLEM : JobType::ADD_PROBLEM);
	mysql
	   .prepare("INSERT jobs(file_id, creator, priority, type, status, added,"
	            " aux_id, info, data) "
	            "VALUES(?,?,?,?," JSTATUS_PENDING_STR ",?,?,?,'')")
	   .bindAndExecute(
	      job_file_id, session_user_id, priority(jtype), jtype, mysql_date(),
	      (reuploading ? Optional<StringView>(problems_pid) : std::nullopt),
	      ap_info.dump());

	auto job_id = mysql.insert_id(); // Has to be retrieved before commit

	transaction.commit();
	job_file_remover.cancel();
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

void Sim::api_statement_impl(uint64_t problem_file_id, StringView problem_label,
                             StringView simfile) {
	STACK_UNWINDING_MARK;

	ConfigFile cf;
	cf.add_vars("statement");
	cf.load_config_from_string(simfile.to_string());

	auto& statement = cf.get_var("statement").as_string();
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

	resp.headers["Content-Disposition"] = concat_tostr(
	   "inline; filename=",
	   http::quote(intentionalUnsafeStringView(concat(problem_label, ext))));

	// TODO: maybe add some cache system for the statements?
	ZipFile zip(internal_file_path(problem_file_id), ZIP_RDONLY);
	resp.content = zip.extract_to_str(
	   zip.get_index(concat(sim::zip_package_master_dir(zip), statement)));
}

void Sim::api_problem_statement(StringView problem_label, StringView simfile) {
	STACK_UNWINDING_MARK;
	using PERM = ProblemPermissions;

	if (uint(~problems_perms & PERM::VIEW_STATEMENT))
		return api_error403();

	return api_statement_impl(problems_file_id, problem_label, simfile);
}

void Sim::api_problem_download(StringView problem_label) {
	STACK_UNWINDING_MARK;
	using PERM = ProblemPermissions;

	if (uint(~problems_perms & PERM::DOWNLOAD))
		return api_error403();

	resp.headers["Content-Disposition"] =
	   concat_tostr("attachment; filename=", problem_label, ".zip");
	resp.content_type = server::HttpResponse::FILE;
	resp.content = internal_file_path(problems_file_id);
}

void Sim::api_problem_rejudge_all_submissions() {
	STACK_UNWINDING_MARK;
	using PERM = ProblemPermissions;

	if (uint(~problems_perms & PERM::REJUDGE_ALL))
		return api_error403();

	mysql
	   .prepare("INSERT jobs (creator, status, priority, type, added, aux_id,"
	            " info, data) "
	            "SELECT ?, " JSTATUS_PENDING_STR ", ?,"
	            " " JTYPE_REJUDGE_SUBMISSION_STR ", ?, id, ?, '' "
	            "FROM submissions WHERE problem_id=? ORDER BY id")
	   .bindAndExecute(session_user_id, priority(JobType::REJUDGE_SUBMISSION),
	                   mysql_date(), jobs::dumpString(problems_pid),
	                   problems_pid);

	jobs::notify_job_server();
}

void Sim::api_problem_reset_time_limits() {
	STACK_UNWINDING_MARK;
	using PERM = ProblemPermissions;

	if (uint(~problems_perms & PERM::RESET_TIME_LIMITS))
		return api_error403();

	mysql
	   .prepare("INSERT jobs (creator, status, priority, type, added, aux_id,"
	            " info, data) "
	            "VALUES(?, " JSTATUS_PENDING_STR ", ?,"
	            " " JTYPE_RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION_STR ","
	            " ?, ?, '', '')")
	   .bindAndExecute(
	      session_user_id,
	      priority(JobType::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION),
	      mysql_date(), problems_pid);

	jobs::notify_job_server();
	append(mysql.insert_id());
}

void Sim::api_problem_delete() {
	STACK_UNWINDING_MARK;
	using PERM = ProblemPermissions;

	if (uint(~problems_perms & PERM::DELETE))
		return api_error403();

	if (not check_submitted_password())
		return api_error403("Invalid password");

	// Queue deleting job
	mysql
	   .prepare("INSERT jobs (creator, status, priority, type, added, aux_id,"
	            " info, data) "
	            "VALUES(?, " JSTATUS_PENDING_STR ", ?,"
	            " " JTYPE_DELETE_PROBLEM_STR ", ?, ?, '', '')")
	   .bindAndExecute(session_user_id, priority(JobType::DELETE_PROBLEM),
	                   mysql_date(), problems_pid);

	jobs::notify_job_server();
	append(mysql.insert_id());
}

void Sim::api_problem_merge_into_another() {
	STACK_UNWINDING_MARK;
	using PERM = ProblemPermissions;

	if (uint(~problems_perms & PERM::MERGE))
		return api_error403();

	InplaceBuff<32> target_problem_id;
	bool rejudge_transferred_submissions =
	   request.form_data.exist("rejudge_transferred_submissions");
	form_validate_not_blank(
	   target_problem_id, "target_problem", "Target problem ID",
	   isDigitNotGreaterThan<std::numeric_limits<decltype(
	      jobs::MergeProblemsInfo::target_problem_id)>::max()>);

	if (problems_pid == target_problem_id)
		return api_error400("You cannot merge problem with itself");

	if (notifications.size)
		return api_error400(notifications);

	auto stmt = mysql.prepare("SELECT owner, type FROM problems WHERE id=?");
	stmt.bindAndExecute(target_problem_id);

	InplaceBuff<32> tp_owner;
	EnumVal<ProblemType> tp_type;
	stmt.res_bind_all(tp_owner, tp_type);

	if (not stmt.next())
		return api_error400("Target problem does not exist");

	ProblemPermissions tp_perms = problems_get_permissions(tp_owner, tp_type);
	if (uint(~tp_perms & PERM::MERGE)) {
		return api_error403(
		   "You do not have permission to merge to the target problem");
	}

	if (not check_submitted_password())
		return api_error403("Invalid password");

	// Queue deleting job
	mysql
	   .prepare("INSERT jobs (creator, status, priority, type, added, aux_id,"
	            " info, data) "
	            "VALUES(?, " JSTATUS_PENDING_STR ", ?,"
	            " " JTYPE_MERGE_PROBLEMS_STR ", ?, ?, ?, '')")
	   .bindAndExecute(session_user_id, priority(JobType::MERGE_PROBLEMS),
	                   mysql_date(), problems_pid,
	                   jobs::MergeProblemsInfo(strtoull(target_problem_id),
	                                           rejudge_transferred_submissions)
	                      .dump());

	jobs::notify_job_server();
	append(mysql.insert_id());
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
		if (notifications.size)
			return api_error400(notifications);

		if (uint(~problems_perms &
		         (hidden ? PERM::EDIT_HIDDEN_TAGS : PERM::EDIT_TAGS)))
			return api_error403();

		auto stmt = mysql.prepare("INSERT IGNORE "
		                          "INTO problem_tags(problem_id, tag, hidden) "
		                          "VALUES(?, ?, ?)");
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
		if (notifications.size)
			return api_error400(notifications);

		if (uint(~problems_perms &
		         (hidden ? PERM::EDIT_HIDDEN_TAGS : PERM::EDIT_TAGS)))
			return api_error403();

		if (name == old_name)
			return;

		auto transaction = mysql.start_transaction();
		auto stmt = mysql.prepare("SELECT 1 FROM problem_tags "
		                          "WHERE tag=? AND problem_id=? AND hidden=?");
		stmt.bindAndExecute(name, problems_pid, hidden);
		if (stmt.next())
			return api_error400("Tag already exist");

		stmt = mysql.prepare("UPDATE problem_tags SET tag=? "
		                     "WHERE problem_id=? AND tag=? AND hidden=?");
		stmt.bindAndExecute(name, problems_pid, old_name, hidden);
		if (stmt.affected_rows() == 0)
			return api_error400("Tag does not exist");

		transaction.commit();
	};

	auto delete_tag = [&] {
		StringView name;
		bool hidden = (request.form_data.get("hidden") == "true");
		form_validate_not_blank(name, "name", "Tag name", PROBLEM_TAG_MAX_LEN);
		if (notifications.size)
			return api_error400(notifications);

		if (uint(~problems_perms &
		         (hidden ? PERM::EDIT_HIDDEN_TAGS : PERM::EDIT_TAGS)))
			return api_error403();

		mysql
		   .prepare("DELETE FROM problem_tags "
		            "WHERE problem_id=? AND tag=? AND hidden=?")
		   .bindAndExecute(problems_pid, name, hidden);
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

void Sim::api_problem_change_statement() {
	STACK_UNWINDING_MARK;
	using PERM = ProblemPermissions;

	if (uint(~problems_perms & PERM::CHANGE_STATEMENT))
		return api_error403();

	CStringView statement_path, statement_file;
	form_validate(statement_path, "path", "New statement path");
	form_validate_file_path_not_blank(statement_file, "statement",
	                                  "New statement");

	if (get_file_size(statement_file) > NEW_STATEMENT_MAX_SIZE) {
		add_notification(
		   "error", "New statement file is too big (maximum allowed size: ",
		   NEW_STATEMENT_MAX_SIZE,
		   " bytes = ", humanizeFileSize(NEW_STATEMENT_MAX_SIZE), ')');
	}

	if (notifications.size)
		return api_error400(notifications);

	jobs::ChangeProblemStatementInfo cps_info(statement_path);

	auto transaction = mysql.start_transaction();
	mysql.update("INSERT INTO internal_files VALUES()");
	auto job_file_id = mysql.insert_id();
	FileRemover job_file_remover(internal_file_path(job_file_id));

	// Make uploaded statement file the job's file
	if (move(statement_file, internal_file_path(job_file_id)))
		THROW("move()", errmsg());

	mysql
	   .prepare("INSERT jobs (file_id, creator, status, priority, type,"
	            " added, aux_id, info, data) "
	            "VALUES(?, ?, " JSTATUS_PENDING_STR ", ?,"
	            " " JTYPE_CHANGE_PROBLEM_STATEMENT_STR ", ?, ?, ?, '')")
	   .bindAndExecute(job_file_id, session_user_id,
	                   priority(JobType::CHANGE_PROBLEM_STATEMENT),
	                   mysql_date(), problems_pid, cps_info.dump());

	auto job_id = mysql.insert_id(); // Has to be retrieved before commit

	transaction.commit();
	job_file_remover.cancel();
	jobs::notify_job_server();

	append(job_id);
}

void Sim::api_problem_attaching_contest_problems() {
	STACK_UNWINDING_MARK;
	using PERM = ProblemPermissions;

	if (uint(~problems_perms & PERM::VIEW_ATTACHING_CONTEST_PROBLEMS))
		return api_error403();

	// We may read data several times (permission checking), so transaction is
	// used to ensure data consistency
	auto transaction = mysql.start_transaction();

	InplaceBuff<512> query;
	query.append("SELECT c.id, c.name, r.id, r.name, p.id, p.name "
	             "FROM contest_problems p "
	             "STRAIGHT_JOIN contests c ON c.id=p.contest_id "
	             "STRAIGHT_JOIN contest_rounds r ON r.id=p.contest_round_id "
	             "WHERE p.problem_id=",
	             problems_pid);

	enum ColumnIdx { CID, CNAME, CRID, CRNAME, CPID, CPNAME };

	// Process restrictions
	auto rows_limit = API_FIRST_QUERY_ROWS_LIMIT;
	StringView next_arg = url_args.extractNextArg();
	for (bool id_condition_occurred = false; next_arg.size();
	     next_arg = url_args.extractNextArg()) {
		auto arg = decodeURI(next_arg);
		char cond = arg[0];
		StringView arg_id = StringView {arg}.substr(1);

		if (not isDigit(arg_id))
			return api_error400();

		// ID condition
		if (isOneOf(cond, '<', '>', '=')) {
			if (id_condition_occurred)
				return api_error400("ID condition specified more than once");

			rows_limit = API_OTHER_QUERY_ROWS_LIMIT;
			query.append(" AND p.id", arg);
			id_condition_occurred = true;

		} else
			return api_error400();
	}

	// Execute query
	query.append(" ORDER BY p.id DESC LIMIT ", rows_limit);
	auto res = mysql.query(query);

	// Column names
	// clang-format off
	append("[\n{\"columns\":["
	           "\"contest_id\","
	           "\"contest_name\","
	           "\"round_id\","
	           "\"round_name\","
	           "\"problem_id\","
	           "\"problem_name\""
	       "]}");
	// clang-format on

	while (res.next()) {
		append(",\n[", res[CID], ",", jsonStringify(res[CNAME]), ",", res[CRID],
		       ",", jsonStringify(res[CRNAME]), ",", res[CPID], ",",
		       jsonStringify(res[CPNAME]), "]");
	}

	append("\n]");
}
