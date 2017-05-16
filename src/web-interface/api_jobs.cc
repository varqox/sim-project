#include "sim.h"

#include <sim/jobs.h>

void Sim::api_job() {
	STACK_UNWINDING_MARK;
	using JQT = JobQueueType;

	if (not session_open())
		return error403(); // Intentionally the whole template

	jobs_job_id = url_args.extractNextArg();

	InplaceBuff<30> jcreator;
	InplaceBuff<256> jinfo;
	uint jtype, jstatus;

	auto stmt = mysql.prepare("SELECT creator, type, status, info"
		" FROM job_queue WHERE id=?");
	stmt.bindAndExecute(jobs_job_id);
	stmt.res_bind_all(jcreator, jtype, jstatus, jinfo);
	if (not stmt.next())
		return api_error404();

	jobs_perms = jobs_get_permissions(jcreator, JobQueueStatus(jstatus));

	StringView next_arg = url_args.extractNextArg();
	if (next_arg == "cancel")
		return api_job_cancel();
	else if (next_arg == "restart")
		return api_job_restart(JQT(jtype), jinfo);
	else if (next_arg == "report")
		return api_job_download_report();
	else if (next_arg == "uploaded-package" and isIn(JQT(jtype), {
		JQT::ADD_PROBLEM,
		JQT::REUPLOAD_PROBLEM,
		JQT::ADD_JUDGE_MODEL_SOLUTION,
		JQT::REUPLOAD_JUDGE_MODEL_SOLUTION}))
	{
		return api_job_download_uploaded_package();

	} else
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
	auto stmt = mysql.prepare("UPDATE job_queue"
		" SET status=" JQSTATUS_CANCELED_STR " WHERE id=?");
	stmt.bindAndExecute(jobs_job_id);
}

void Sim::api_job_restart(JobQueueType job_type, StringView job_info) {
	STACK_UNWINDING_MARK;
	using PERM = JobPermissions;
	using JQT = JobQueueType;

	if (request.method != server::HttpRequest::POST)
		return api_error400();

	if (uint(~jobs_perms & PERM::RESTART)) {
		if (uint(jobs_perms & PERM::VIEW))
			return api_error400("Job has already been restarted");
		return api_error403();
	}

	/* Restart job */

	// Restart adding / reuploading problem
	bool adding = isIn(job_type,
		{JQT::ADD_PROBLEM, JQT::ADD_JUDGE_MODEL_SOLUTION});
	bool reupload = isIn(job_type,
		{JQT::REUPLOAD_PROBLEM, JQT::REUPLOAD_JUDGE_MODEL_SOLUTION});

	if (adding or reupload) {
		jobs::AddProblemInfo info {job_info};
		info.stage = jobs::AddProblemInfo::FIRST;

		auto stmt = mysql.prepare("UPDATE job_queue"
			" SET type=?, status=" JQSTATUS_PENDING_STR ", info=?"
			" WHERE id=?");

		stmt.bindAndExecute(
			uint(adding ? JQT::ADD_PROBLEM : JQT::REUPLOAD_PROBLEM),
			info.dump(), jobs_job_id);

	// Restart job of other type
	} else {
		auto stmt = mysql.prepare("UPDATE job_queue"
			" SET status=" JQSTATUS_PENDING_STR " WHERE id=?");
		stmt.bindAndExecute(jobs_job_id);
	}

	notify_job_server();
}

void Sim::api_job_download_report() {
	STACK_UNWINDING_MARK;

	// Assumption: permissions are already checked
	resp.headers["Content-type"] = "application/text";
	resp.headers["Content-Disposition"] =
		concat_tostr("attachment; filename=job-", jobs_job_id, "-report");

	// Fetch the report
	auto stmt = mysql.prepare("SELECT data FROM job_queue WHERE id=?");
	stmt.bindAndExecute(jobs_job_id);
	stmt.res_bind_all(resp.content);
	throw_assert(stmt.next());
}

void Sim::api_job_download_uploaded_package() {
	STACK_UNWINDING_MARK;

	resp.headers["Content-Disposition"] =
		concat_tostr("attachment; filename=", jobs_job_id, ".zip");
	resp.content_type = server::HttpResponse::FILE;
	resp.content = concat("jobs_files/", jobs_job_id, ".zip");
}

static constexpr const char* job_type_str(JobQueueType type) noexcept {
	using T = JobQueueType;

	switch (type) {
	case T::JUDGE_SUBMISSION: return "Judge submission";
	case T::ADD_PROBLEM: return "Add problem";
	case T::REUPLOAD_PROBLEM: return "Reupload problem";
	case T::ADD_JUDGE_MODEL_SOLUTION: return "Add problem - set limits";
	case T::REUPLOAD_JUDGE_MODEL_SOLUTION:
		return"Reupload problem - set limits";
	case T::EDIT_PROBLEM: return "Edit problem";
	case T::DELETE_PROBLEM: return "Delete problem";
	case T::VOID: return "Void";
	}
	return "Unknown";
}

void Sim::api_jobs() {
	STACK_UNWINDING_MARK;

	if (not session_open())
		return api_error403();

	using PERM = JobPermissions;

	// Get permissions to the overall job queue
	jobs_perms = jobs_get_permissions("", JobQueueStatus::DONE);
	StringView next_arg = url_args.extractNextArg();
	bool my_jobs = false, select_data = false;

	InplaceArray<InplaceBuff<30>, 5> to_bind;
	InplaceBuff<512> qfields, qwhere;
	qfields.append("SELECT j.id, added, j.type, j.status, j.priority, j.aux_id,"
			" j.info, j.creator, u.username");
	qwhere.append(" FROM job_queue j LEFT JOIN users u ON creator=u.id"
		" WHERE j.type!=" JQTYPE_VOID_STR);

	bool allow_access = true;
	// Request only for user's jobs
	if (next_arg == "my") {
		next_arg = url_args.extractNextArg();
		qwhere.append(" AND creator=?");
		to_bind.emplace_back(session_user_id);
		my_jobs = true;
	// Request for all jobs
	} else
		allow_access &= bool(uint(jobs_perms & PERM::VIEW_ALL));

	MySQL::Statement<> stmt;

	// Process restrictions
	for (uint i = 0, mask = 0;
		i < 2 and next_arg.size();
		++i, next_arg = url_args.extractNextArg())
	{
		constexpr uint ID_COND = 1;
		constexpr uint AUX_ID_COND = 2;

		auto arg = decodeURI(next_arg);
		char cond = arg[0];
		StringView arg_id = StringView{arg}.substr(1);

		if (not isDigit(arg_id))
			return api_error400();

		// conditional
		if (cond == '<' and ~mask & ID_COND) {
			qwhere.append(" AND j.id<?");
			to_bind.emplace_back(arg_id);
			mask |= ID_COND;

		} else if (cond == '=' and ~mask & ID_COND) {
			qfields.append(", SUBSTR(data, 1, ",
				meta::ToString<REPORT_PREVIEW_MAX_LENGTH + 1>{}, ')');
			select_data = true;
			qwhere.append(" AND j.id=?");
			to_bind.emplace_back(arg_id);
			mask |= ID_COND;

		// problem
		} else if (cond == 'p' and ~mask & AUX_ID_COND) {
			// Check permissions - they may be granted
			if (not allow_access) {
				stmt = mysql.prepare("SELECT owner, type FROM problems"
					" WHERE id=?");
				stmt.bindAndExecute(arg_id);

				InplaceBuff<30> powner;
				uint ptype;
				stmt.res_bind_all(powner, ptype);
				if (stmt.next()) {
					auto pperms = problems_get_permissions(powner,
						ProblemType(ptype));
					allow_access |=
						bool(uint(pperms & ProblemPermissions::ADMIN));
				}
			}

			qwhere.append(" AND j.aux_id=? AND j.type IN("
				JQTYPE_ADD_PROBLEM_STR ","
				JQTYPE_ADD_JUDGE_MODEL_SOLUTION_STR ","
				JQTYPE_REUPLOAD_PROBLEM_STR ","
				JQTYPE_REUPLOAD_JUDGE_MODEL_SOLUTION_STR ","
				JQTYPE_DELETE_PROBLEM_STR ","
				JQTYPE_EDIT_PROBLEM_STR ")");

			to_bind.emplace_back(arg_id);
			mask |= AUX_ID_COND;

		// submission
		} else if (cond == 's' and ~mask & AUX_ID_COND) {
			qwhere.append(" AND j.aux_id=? AND j.type="
				JQTYPE_JUDGE_SUBMISSION_STR);

			to_bind.emplace_back(arg_id);
			mask |= AUX_ID_COND;

		} else
			return api_error400();

	}

	if (not allow_access)
		return api_error403();

	// Execute query
	stmt =
		mysql.prepare(concat(qfields, qwhere, " ORDER BY j.id DESC LIMIT 50"));

	for (uint i = 0; i < to_bind.size(); ++i)
		stmt.bind(i, to_bind[i]);
	stmt.fixBinds();
	stmt.execute();

	resp.headers["content-type"] = "text/plain; charset=utf-8";
	append("[");

	uint8_t jtype, jstatus;
	my_bool is_aux_id_null, is_creator_null;
	InplaceBuff<30> job_id, added, jpriority, aux_id, creator;
	InplaceBuff<512> jinfo;
	InplaceBuff<USERNAME_MAX_LEN> cusername;
	InplaceBuff<REPORT_PREVIEW_MAX_LENGTH> report_preview;

	stmt.res_bind(0, job_id);
	stmt.res_bind(1, added);
	stmt.res_bind(2, jtype);
	stmt.res_bind(3, jstatus);
	stmt.res_bind(4, jpriority);
	stmt.res_bind(5, aux_id);
	stmt.res_bind_isnull(5, is_aux_id_null);
	stmt.res_bind(6, jinfo);
	stmt.res_bind(7, creator);
	stmt.res_bind_isnull(7, is_creator_null);
	stmt.res_bind(8, cusername);
	if (select_data)
		stmt.res_bind(9, report_preview);
	stmt.resFixBinds();

	while (stmt.next()) {
		JobQueueType job_type {JobQueueType(jtype)};
		JobQueueStatus job_status {JobQueueStatus(jstatus)};

		append("\n[", job_id, ',',
			jsonStringify(added), ","
			"\"", job_type_str(job_type), "\",");

		// Status: (CSS class, text)
		switch (job_status) {
		case JobQueueStatus::PENDING: append("[\"\",\"Pending\"],"); break;
		case JobQueueStatus::IN_PROGRESS:
			append("[\"yellow\",\"In progress\"],"); break;
		case JobQueueStatus::DONE: append("[\"green\",\"Done\"],"); break;
		case JobQueueStatus::FAILED: append("[\"red\",\"Failed\"],"); break;
		case JobQueueStatus::CANCELED:
			append("[\"blue\",\"Cancelled\"],"); break;
		}

		append(jpriority, ',');

		// Creator
		if (is_creator_null)
			append("null,null,");
		else
			append(creator, ",\"", cusername, "\",");

		// Additional info
		InplaceBuff<10> actions;
		append('{');

		switch (job_type) {
		case JobQueueType::JUDGE_SUBMISSION: {
			StringView x {jinfo};
			append("\"problem\":", jobs::extractDumpedString(x));
			append(",\"submission\":", aux_id);

			actions.append('R'); // Download report
			break;
		}

		case JobQueueType::ADD_PROBLEM:
		case JobQueueType::REUPLOAD_PROBLEM:
		case JobQueueType::ADD_JUDGE_MODEL_SOLUTION:
		case JobQueueType::REUPLOAD_JUDGE_MODEL_SOLUTION: {
			jobs::AddProblemInfo info {jinfo};
			append("\"make public\":", info.make_public ?
				"\"yes\"" : "\"no\"");

			if (info.name.size())
				append(",\"name\":", jsonStringify(info.name));
			if (info.label.size())
				append(",\"label\":", jsonStringify(info.label));
			if (info.memory_limit)
				append(",\"memory limit\":\"", info.memory_limit, " MB\"");
			if (info.global_time_limit)
				append(",\"global time limit\":",
					usecToSecStr(info.global_time_limit, 6));

			append(",\"auto time limit setting\":", info.force_auto_limit ?
					"\"yes\"" : "\"no\"",
				",\"ignore simfile\":", info.ignore_simfile ?
					"\"yes\"" : "\"no\"");

			if (not is_aux_id_null)
				append(",\"problem\":", aux_id);

			// Add actions
			actions.append('P'); // Download uploaded package

			if (not is_aux_id_null)
				actions.append('V'); // View problem
			break;
		}

		default:
			break;
		}
		append("},");


		// Append what buttons to show
		append('"', actions);

		auto job_perms = jobs_get_permissions(my_jobs ? session_user_id
			: creator, job_status);
		if (uint(job_perms & PERM::CANCEL))
			append('c');
		if (uint(job_perms & PERM::RESTART))
			append('r');
		append('\"');

		// Append report preview (whether there is more to load, data)
		if (select_data)
			append(",[", report_preview.size > REPORT_PREVIEW_MAX_LENGTH, ',',
				jsonStringify(report_preview), ']');

		append("],");
	}

	if (resp.content.back() == ',')
		--resp.content.size;

	append("\n]");
}
