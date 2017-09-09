#include "sim.h"

// #include <sim/jobs.h>

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

	// Get permissions to the overall job queue
	problems_perms = problems_get_permissions();
	// Choose problems to select
	if (not uint(problems_perms & PERM::ADMIN)) {
		if (session_user_type == UserType::TEACHER) {
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
		if (isIn(cond, "<>") and ~mask & ID_COND) {
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
			if (arg_id != session_user_id and
				not uint(problems_perms & PERM::SELECT_BY_OWNER))
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

	resp.headers["content-type"] = "text/plain; charset=utf-8";
	append("[");

	// Tags selector
	uint64_t pid;
	bool hidden;
	InplaceBuff<PROBLEM_TAG_MAX_LEN> tag;
	auto stmt = mysql.prepare("SELECT tag FROM problems_tags"
		" WHERE problem_id=? AND hidden=? ORDER BY tag");
	stmt.bind_all(pid, hidden);
	stmt.res_bind_all(tag);

	while (res.next()) {
		ProblemType problem_type {ProblemType(strtoull(res[2]))};
		// res[5] == owner
		auto perms = problems_get_permissions(res[5], problem_type);

		// Id
		append("\n[", res[0], ",");

		// Added
		if (uint(perms & PERM::VIEW_ADD_TIME))
			append("\"", res[1], "\",");
		else
			append("null,");

		// Type
		append("\"", proiblem_type_str(problem_type), "\",");
		// Name
		append(jsonStringify(res[3]), ',');
		// Label
		append(jsonStringify(res[4]), ',');

		if (uint(perms & PERM::VIEW_OWNER)) {
			// Owner
			append(res[5], ',');

			// Owner's username
			if (res.is_null(6))
				append("null,");
			else
				append("\"", res[6], "\","); // username
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
			pid = strtoull(res[0]);
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

		// Append simfile
		if (select_specified_problem and uint(perms & PERM::VIEW_SIMFILE))
			append(',', jsonStringify(res[7])); // simfile

		append("],");
	}

	if (resp.content.back() == ',')
		--resp.content.size;

	append("\n]");
}

#if 1-1
void Sim::api_job() {
	STACK_UNWINDING_MARK;
	using JT = JobType;

	if (not session_open())
		return api_error403();

	jobs_job_id = url_args.extractNextArg();
	if (not isDigit(jobs_job_id))
		return api_error400();

	InplaceBuff<32> jcreator;
	InplaceBuff<256> jinfo;
	uint jtype, jstatus;

	auto stmt = mysql.prepare("SELECT creator, type, status, info FROM jobs"
		" WHERE id=?");
	stmt.bindAndExecute(jobs_job_id);
	stmt.res_bind_all(jcreator, jtype, jstatus, jinfo);
	if (not stmt.next())
		return api_error404();

	jobs_perms = jobs_get_permissions(jcreator, JobType(jtype),
		JobStatus(jstatus));

	StringView next_arg = url_args.extractNextArg();
	if (next_arg == "cancel")
		return api_job_cancel();
	else if (next_arg == "restart")
		return api_job_restart(JT(jtype), jinfo);
	else if (next_arg == "report")
		return api_job_download_report();
	else if (next_arg == "uploaded-package")
		return api_job_download_uploaded_package();
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
	auto stmt = mysql.prepare("UPDATE jobs SET status=" JSTATUS_CANCELED_STR
		" WHERE id=?");
	stmt.bindAndExecute(jobs_job_id);
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

	jobs::restart_job(mysql, jobs_job_id, job_type, job_info, true);
}

void Sim::api_job_download_report() {
	STACK_UNWINDING_MARK;
	using PERM = JobPermissions;

	if (uint(~jobs_perms & PERM::DOWNLOAD_REPORT))
		return api_error403();

	// Assumption: permissions are already checked
	resp.headers["Content-type"] = "application/text";
	resp.headers["Content-Disposition"] =
		concat_tostr("attachment; filename=job-", jobs_job_id, "-report");

	// Fetch the report
	auto stmt = mysql.prepare("SELECT data FROM jobs WHERE id=?");
	stmt.bindAndExecute(jobs_job_id);
	stmt.res_bind_all(resp.content);
	throw_assert(stmt.next());
}

void Sim::api_job_download_uploaded_package() {
	STACK_UNWINDING_MARK;
	using PERM = JobPermissions;

	if (uint(~jobs_perms & PERM::DOWNLOAD_UPLOADED_PACKAGE))
		return api_error403();

	resp.headers["Content-Disposition"] =
		concat_tostr("attachment; filename=", jobs_job_id, ".zip");
	resp.content_type = server::HttpResponse::FILE;
	resp.content = concat("jobs_files/", jobs_job_id, ".zip");
}
#endif