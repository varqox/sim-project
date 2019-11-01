#include "sim.h"

#include <sim/jobs.h>
#include <simlib/filesystem.h>

static constexpr const char* job_type_str(JobType type) noexcept {
	using JT = JobType;

	switch (type) {
	case JT::JUDGE_SUBMISSION: return "Judge submission";
	case JT::REJUDGE_SUBMISSION: return "Rejudge submission";
	case JT::ADD_PROBLEM: return "Add problem";
	case JT::REUPLOAD_PROBLEM: return "Reupload problem";
	case JT::ADD_PROBLEM__JUDGE_MODEL_SOLUTION:
		return "Add problem - set time limits";
	case JT::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION:
		return "Reupload problem - set time limits";
	case JT::EDIT_PROBLEM: return "Edit problem";
	case JT::DELETE_PROBLEM: return "Delete problem";
	case JT::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
		return "Reselect final submissions in a contest problem";
	case JT::DELETE_USER: return "Delete user";
	case JT::DELETE_CONTEST: return "Delete contest";
	case JT::DELETE_CONTEST_ROUND: return "Delete contest round";
	case JT::DELETE_CONTEST_PROBLEM: return "Delete contest problem";
	case JT::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
		return "Reset problem time limits using model solution";
	case JT::MERGE_PROBLEMS: return "Merge problems";
	case JT::DELETE_FILE: return "Delete internal file";
	case JT::CHANGE_PROBLEM_STATEMENT: return "Change problem statement";
	}

	return "Unknown";
}

void Sim::api_jobs() {
	STACK_UNWINDING_MARK;

	if (not session_is_open)
		return api_error403();

	using PERM = JobPermissions;

	// We may read data several times (permission checking), so transaction is
	// used to ensure data consistency
	auto transaction = mysql.start_transaction();

	// Get the overall permissions to the job queue
	jobs_perms = jobs_get_overall_permissions();

	InplaceBuff<512> qfields, qwhere;
	qfields.append("SELECT j.id, added, j.type, j.status, j.priority, j.aux_id,"
	               " j.info, j.creator, u.username");
	qwhere.append(
	   " FROM jobs j LEFT JOIN users u ON creator=u.id WHERE TRUE"); // Needed
	                                                                 // to
	                                                                 // easily
	                                                                 // append
	                                                                 // constraints

	enum ColumnIdx {
		JID,
		ADDED,
		JTYPE,
		JSTATUS,
		PRIORITY,
		AUX_ID,
		JINFO,
		CREATOR,
		CREATOR_USERNAME,
		JOB_LOG_VIEW
	};

	auto append_column_names = [&] {
		// clang-format off
		append("[\n{\"columns\":["
		           "\"id\","
		           "\"added\","
		           "\"type\","
		           "{\"name\":\"status\",\"fields\":[\"class\",\"text\"]},"
		           "\"priority\","
		           "\"creator_id\","
		           "\"creator_username\","
		           "\"info\","
		           "\"actions\","
		           "{\"name\":\"log\",\"fields\":[\"is_incomplete\",\"text\"]}"
		       "]}");
		// clang-format on
	};

	auto set_empty_response = [&] {
		resp.content.clear();
		append_column_names();
		append("\n]");
	};

	bool allow_access = uint(jobs_perms & PERM::VIEW_ALL);
	bool select_specified_job = false;

	PERM granted_perms = PERM::NONE;

	// Process restrictions
	auto rows_limit = API_FIRST_QUERY_ROWS_LIMIT;
	StringView next_arg = url_args.extractNextArg();
	for (uint mask = 0; next_arg.size(); next_arg = url_args.extractNextArg()) {
		constexpr uint ID_COND = 1;
		constexpr uint AUX_ID_COND = 2;
		constexpr uint USER_ID_COND = 4;

		auto arg = decodeURI(next_arg);
		char cond = arg[0];
		StringView arg_id = StringView(arg).substr(1);

		if (not isDigit(arg_id))
			return api_error400();

		// conditional
		if (is_one_of(cond, '<', '>') and ~mask & ID_COND) {
			rows_limit = API_OTHER_QUERY_ROWS_LIMIT;
			qwhere.append(" AND j.id", arg);
			mask |= ID_COND;

		} else if (cond == '=' and ~mask & ID_COND) {
			select_specified_job = true;
			// Get job information to grant permissions
			EnumVal<JobType> jtype;
			InplaceBuff<32> aux_id;
			auto stmt =
			   mysql.prepare("SELECT type, aux_id FROM jobs WHERE id=?");
			stmt.bindAndExecute(arg_id);
			stmt.res_bind_all(jtype, aux_id);
			if (not stmt.next())
				return api_error404();

			// Grant permissions if possible
			if (is_problem_job(jtype))
				granted_perms |= jobs_granted_permissions_problem(aux_id);
			else if (is_submission_job(jtype))
				granted_perms |= jobs_granted_permissions_submission(aux_id);
			allow_access |= (granted_perms != PERM::NONE);

			if (not allow_access) {
				// If user cannot view all jobs, allow them to view their jobs
				allow_access = true;
				qwhere.append(" AND creator=", session_user_id);
			}

			qfields.append(", SUBSTR(data, 1, ",
			               meta::ToString<JOB_LOG_VIEW_MAX_LENGTH + 1> {}, ')');
			qwhere.append(" AND j.id", arg);
			mask |= ID_COND;

		} else if (cond == 'p' and ~mask & AUX_ID_COND) { // Problem
			// Check permissions - they may be granted
			granted_perms |= jobs_granted_permissions_problem(arg_id);
			allow_access |= (granted_perms != PERM::NONE);

			qwhere.append(" AND j.aux_id=", arg_id,
			              " AND j.type IN(" JTYPE_ADD_PROBLEM_STR
			              "," JTYPE_ADD_PROBLEM__JUDGE_MODEL_SOLUTION_STR
			              "," JTYPE_REUPLOAD_PROBLEM_STR
			              "," JTYPE_REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION_STR
			              "," JTYPE_DELETE_PROBLEM_STR
			              "," JTYPE_EDIT_PROBLEM_STR ")");

			mask |= AUX_ID_COND;

		} else if (cond == 's' and ~mask & AUX_ID_COND) { // Submission
			// Check permissions - they may be granted
			granted_perms |= jobs_granted_permissions_submission(arg_id);
			allow_access |= (granted_perms != PERM::NONE);

			qwhere.append(" AND j.aux_id=", arg_id,
			              " AND (j.type=" JTYPE_JUDGE_SUBMISSION_STR
			              " OR j.type=" JTYPE_REJUDGE_SUBMISSION_STR ")");

			mask |= AUX_ID_COND;

		} else if (cond == 'u' and ~mask & USER_ID_COND) { // User (creator)
			qwhere.append(" AND creator=", arg_id);
			if (arg_id == session_user_id)
				allow_access = true;

			mask |= USER_ID_COND;

		} else {
			return api_error400();
		}
	}

	if (not allow_access)
		return set_empty_response();

	// Execute query
	qfields.append(qwhere, " ORDER BY j.id DESC LIMIT ", rows_limit);
	auto res = mysql.query(qfields);

	append_column_names();

	while (res.next()) {
		JobType job_type {JobType(strtoull(res[JTYPE]))};
		JobStatus job_status {JobStatus(strtoull(res[JSTATUS]))};

		// clang-format off
		append(",\n[", res[JID], ","
		       "\"", res[ADDED], "\","
		       "\"", job_type_str(job_type), "\",");
		// clang-format on

		// Status: (CSS class, text)
		switch (job_status) {
		case JobStatus::PENDING: append("[\"\",\"Pending\"],"); break;
		case JobStatus::NOTICED_PENDING: append("[\"\",\"Pending\"],"); break;
		case JobStatus::IN_PROGRESS:
			append("[\"yellow\",\"In progress\"],");
			break;
		case JobStatus::DONE: append("[\"green\",\"Done\"],"); break;
		case JobStatus::FAILED: append("[\"red\",\"Failed\"],"); break;
		case JobStatus::CANCELED: append("[\"blue\",\"Canceled\"],"); break;
		}

		append(res[PRIORITY], ',');

		// Creator
		if (res.is_null(CREATOR))
			append("null,");
		else
			append(res[CREATOR], ',');

		// Username
		if (res.is_null(CREATOR_USERNAME))
			append("null,");
		else
			append("\"", res[CREATOR_USERNAME], "\",");

		// Additional info
		InplaceBuff<10> actions;
		append('{');

		switch (job_type) {
		case JobType::JUDGE_SUBMISSION:
		case JobType::REJUDGE_SUBMISSION: {
			append("\"problem\":", jobs::extractDumpedString(res[JINFO]));
			append(",\"submission\":", res[AUX_ID]);
			break;
		}

		case JobType::ADD_PROBLEM:
		case JobType::REUPLOAD_PROBLEM:
		case JobType::ADD_PROBLEM__JUDGE_MODEL_SOLUTION:
		case JobType::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION: {
			auto ptype_to_str = [](ProblemType& ptype) {
				switch (ptype) {
				case ProblemType::PUBLIC: return "public";
				case ProblemType::PRIVATE: return "private";
				case ProblemType::CONTEST_ONLY: return "contest only";
				}
				return "unknown";
			};
			jobs::AddProblemInfo info {res[JINFO]};
			append("\"problem type\":\"", ptype_to_str(info.problem_type), '"');

			if (info.name.size())
				append(",\"name\":", jsonStringify(info.name));
			if (info.label.size())
				append(",\"label\":", jsonStringify(info.label));
			if (info.memory_limit.has_value())
				append(",\"memory limit\":\"", info.memory_limit.value(),
				       " MiB\"");
			if (info.global_time_limit.has_value())
				append(",\"global time limit\":",
				       toString(info.global_time_limit.value()));

			append(",\"reset time limits\":",
			       info.reset_time_limits ? "\"yes\"" : "\"no\"",
			       ",\"ignore simfile\":",
			       info.ignore_simfile ? "\"yes\"" : "\"no\"",
			       ",\"seek for new tests\":",
			       info.seek_for_new_tests ? "\"yes\"" : "\"no\"",
			       ",\"reset scoring\":",
			       info.reset_scoring ? "\"yes\"" : "\"no\"");

			if (not res.is_null(AUX_ID)) {
				append(",\"problem\":", res[AUX_ID]);
				actions.append('p'); // View problem
			}

			break;
		}

		case JobType::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM: {
			append("\"contest problem\":", res[AUX_ID]);
			break;
		}

		case JobType::DELETE_PROBLEM: {
			append("\"problem\":", res[AUX_ID]);
			break;
		}

		case JobType::DELETE_USER: {
			append("\"user\":", res[AUX_ID]);
			break;
		}

		case JobType::DELETE_CONTEST: {
			append("\"contest\":", res[AUX_ID]);
			break;
		}

		case JobType::DELETE_CONTEST_ROUND: {
			append("\"contest round\":", res[AUX_ID]);
			break;
		}

		case JobType::DELETE_CONTEST_PROBLEM: {
			append("\"contest problem\":", res[AUX_ID]);
			break;
		}

		case JobType::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION: {
			append("\"problem\":", res[AUX_ID]);
			break;
		}

		case JobType::MERGE_PROBLEMS: {
			append("\"deleted problem\":", res[AUX_ID]);
			jobs::MergeProblemsInfo info(res[JINFO]);
			append(",\"target problem\":", info.target_problem_id);
			append(",\"rejudge transferred submissions\":",
			       info.rejudge_transferred_submissions ? "\"yes\"" : "\"no\"");
			break;
		}

		case JobType::CHANGE_PROBLEM_STATEMENT: {
			append("\"problem\":", res[AUX_ID]);
			jobs::ChangeProblemStatementInfo info(res[JINFO]);
			append(",\"new statement path\":",
			       jsonStringify(info.new_statement_path));
			break;
		}

		case JobType::DELETE_FILE: break; // Nothing to show

		case JobType::EDIT_PROBLEM: break;
		}
		append("},");

		// Append what buttons to show
		append('"', actions);

		auto perms = granted_perms |
		             jobs_get_permissions(res[CREATOR], job_type, job_status);
		if (uint(perms & PERM::VIEW))
			append('v');
		if (uint(perms & PERM::DOWNLOAD_LOG))
			append('r');
		using JT = JobType;
		if (uint(perms & PERM::DOWNLOAD_UPLOADED_PACKAGE) and
		    is_one_of(job_type, JT::ADD_PROBLEM, JT::REUPLOAD_PROBLEM,
		              JT::ADD_PROBLEM__JUDGE_MODEL_SOLUTION,
		              JT::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION)) {
			append('u'); // TODO: ^ that is very nasty
		}
		if (uint(perms & PERM::DOWNLOAD_UPLOADED_STATEMENT) and
		    job_type == JT::CHANGE_PROBLEM_STATEMENT)
			append('s'); // TODO: ^ that is very nasty
		if (uint(perms & PERM::CANCEL))
			append('C');
		if (uint(perms & PERM::RESTART))
			append('R');
		append('\"');

		// Append log view (whether there is more to load, data)
		if (select_specified_job and uint(perms & PERM::DOWNLOAD_LOG))
			append(",[", res[JOB_LOG_VIEW].size() > JOB_LOG_VIEW_MAX_LENGTH,
			       ',', jsonStringify(res[JOB_LOG_VIEW]), ']');

		append(']');
	}

	append("\n]");
}

void Sim::api_job() {
	STACK_UNWINDING_MARK;
	using JT = JobType;

	if (not session_is_open)
		return api_error403();

	jobs_jid = url_args.extractNextArg();
	if (not isDigit(jobs_jid))
		return api_error400();

	MySQL::Optional<uint64_t> file_id;
	MySQL::Optional<InplaceBuff<32>> jcreator;
	InplaceBuff<32> aux_id;
	InplaceBuff<256> jinfo;
	EnumVal<JT> jtype;
	EnumVal<JobStatus> jstatus;

	auto stmt =
	   mysql.prepare("SELECT file_id, creator, type, status, aux_id, info "
	                 "FROM jobs WHERE id=?");
	stmt.bindAndExecute(jobs_jid);
	stmt.res_bind_all(file_id, jcreator, jtype, jstatus, aux_id, jinfo);
	if (not stmt.next())
		return api_error404();

	{
		std::optional<StringView> creator;
		if (jcreator.has_value())
			creator = jcreator.value();
		jobs_perms = jobs_get_permissions(creator, jtype, jstatus);
	}
	// Grant permissions if possible
	if (is_problem_job(jtype))
		jobs_perms |= jobs_granted_permissions_problem(aux_id);
	else if (is_submission_job(jtype))
		jobs_perms |= jobs_granted_permissions_submission(aux_id);

	StringView next_arg = url_args.extractNextArg();
	if (next_arg == "cancel")
		return api_job_cancel();
	else if (next_arg == "restart")
		return api_job_restart(jtype, jinfo);
	else if (next_arg == "log")
		return api_job_download_log();
	else if (next_arg == "uploaded-package")
		return api_job_download_uploaded_package(file_id, jtype);
	else if (next_arg == "uploaded-statement")
		return api_job_download_uploaded_statement(file_id, jtype, jinfo);
	else
		return api_error400();
}

void Sim::api_job_cancel() {
	STACK_UNWINDING_MARK;
	using PERM = JobPermissions;

	if (request.method != server::HttpRequest::POST)
		return api_error400();

	if (uint(~jobs_perms & PERM::CANCEL)) {
		if (uint(jobs_perms & PERM::VIEW))
			return api_error400("Job has already been canceled or done");
		return api_error403();
	}

	// Cancel job
	mysql.prepare("UPDATE jobs SET status=" JSTATUS_CANCELED_STR " WHERE id=?")
	   .bindAndExecute(jobs_jid);
}

void Sim::api_job_restart(JobType job_type, StringView job_info) {
	STACK_UNWINDING_MARK;
	using PERM = JobPermissions;

	if (request.method != server::HttpRequest::POST)
		return api_error400();

	if (uint(~jobs_perms & PERM::RESTART)) {
		if (uint(jobs_perms & PERM::VIEW))
			return api_error400("Job has already been restarted");
		return api_error403();
	}

	jobs::restart_job(mysql, jobs_jid, job_type, job_info, true);
}

void Sim::api_job_download_log() {
	STACK_UNWINDING_MARK;
	using PERM = JobPermissions;

	if (uint(~jobs_perms & PERM::DOWNLOAD_LOG))
		return api_error403();

	// Assumption: permissions are already checked
	resp.headers["Content-type"] = "application/text";
	resp.headers["Content-Disposition"] =
	   concat_tostr("attachment; filename=job-", jobs_jid, "-log");

	// Fetch the log
	auto stmt = mysql.prepare("SELECT data FROM jobs WHERE id=?");
	stmt.bindAndExecute(jobs_jid);
	stmt.res_bind_all(resp.content);
	throw_assert(stmt.next());
}

void Sim::api_job_download_uploaded_package(std::optional<uint64_t> file_id,
                                            JobType job_type) {
	STACK_UNWINDING_MARK;
	using PERM = JobPermissions;
	using JT = JobType;

	if (uint(~jobs_perms & PERM::DOWNLOAD_UPLOADED_PACKAGE) or
	    not is_one_of(job_type, JT::ADD_PROBLEM, JT::REUPLOAD_PROBLEM,
	                  JT::ADD_PROBLEM__JUDGE_MODEL_SOLUTION,
	                  JT::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION)) {
		return api_error403(); // TODO: ^ that is very nasty
	}

	resp.headers["Content-Disposition"] =
	   concat_tostr("attachment; filename=", jobs_jid, ".zip");
	resp.content_type = server::HttpResponse::FILE;
	resp.content = internal_file_path(file_id.value());
}

void Sim::api_job_download_uploaded_statement(std::optional<uint64_t> file_id,
                                              JobType job_type,
                                              StringView info) {
	STACK_UNWINDING_MARK;
	using PERM = JobPermissions;
	using JT = JobType;

	if (uint(~jobs_perms & PERM::DOWNLOAD_UPLOADED_STATEMENT) or
	    job_type != JT::CHANGE_PROBLEM_STATEMENT) {
		return api_error403(); // TODO: ^ that is very nasty
	}

	resp.headers["Content-Disposition"] = concat_tostr(
	   "attachment; filename=",
	   encodeURI(intentionalUnsafeStringView(filename(
	      jobs::ChangeProblemStatementInfo(info).new_statement_path))));
	resp.content_type = server::HttpResponse::FILE;
	resp.content = internal_file_path(file_id.value());
}
