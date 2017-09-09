#include "sim.h"

#include <sim/jobs.h>

using std::string;

Sim::JobPermissions Sim::jobs_get_permissions(StringView creator_id,
	JobType job_type, JobStatus job_status)
{
	STACK_UNWINDING_MARK;
	using PERM = JobPermissions;
	using JT = JobType;
	using JS = JobStatus;

	D(throw_assert(session_is_open);) // Session must be open to access jobs

	JobPermissions perm = (isIn(job_type, {JT::ADD_PROBLEM,
		JT::REUPLOAD_PROBLEM, JT::ADD_JUDGE_MODEL_SOLUTION,
		JT::REUPLOAD_JUDGE_MODEL_SOLUTION})
		? PERM::DOWNLOAD_REPORT | PERM::DOWNLOAD_UPLOADED_PACKAGE : PERM::NONE);

	if (session_user_type == UserType::ADMIN) {
		switch (job_status) {
		case JS::PENDING:
		case JS::NOTICED_PENDING:
		case JS::IN_PROGRESS:
			return perm | PERM::VIEW | PERM::DOWNLOAD_REPORT | PERM::VIEW_ALL |
				PERM::CANCEL;

		case JS::FAILED:
		case JS::CANCELED:
			return perm | PERM::VIEW | PERM::DOWNLOAD_REPORT | PERM::VIEW_ALL |
				PERM::RESTART;

		default:
			return perm | PERM::VIEW | PERM::DOWNLOAD_REPORT | PERM::VIEW_ALL;
		}
	}

	static_assert(uint(PERM::NONE) == 0, "Needed below");
	if (session_user_id == creator_id)
		return perm | PERM::VIEW | (isIn(job_status,
			{JS::PENDING, JS::NOTICED_PENDING, JS::IN_PROGRESS}) ?
				PERM::CANCEL : PERM::NONE);

	return PERM::NONE;
}

Sim::JobPermissions Sim::jobs_granted_permissions_problem(StringView problem_id)
{
	STACK_UNWINDING_MARK;
	using PERM = JobPermissions;

	auto stmt = mysql.prepare("SELECT owner, type FROM problems WHERE id=?");
	stmt.bindAndExecute(problem_id);

	InplaceBuff<32> powner;
	uint ptype;
	stmt.res_bind_all(powner, ptype);
	if (stmt.next()) {
		auto pperms = problems_get_permissions(powner, ProblemType(ptype));
		if (uint(pperms & ProblemPermissions::VIEW_RELATED_JOBS))
			return PERM::VIEW | PERM::DOWNLOAD_REPORT |
				PERM::DOWNLOAD_UPLOADED_PACKAGE;
	}

	return PERM::NONE;
}

Sim::JobPermissions Sim::jobs_granted_permissions_submission(
	StringView submission_id)
{
	STACK_UNWINDING_MARK;
	using PERM = JobPermissions;

	auto stmt = mysql.prepare("SELECT s.type, p.owner, p.type"
		" FROM submissions s STRAIGHT_JOIN problems p ON p.id=s.problem_id"
		" WHERE s.id=?");
	stmt.bindAndExecute(submission_id);

	InplaceBuff<32> powner;
	uint stype, ptype;
	stmt.res_bind_all(stype, powner, ptype);
	if (stmt.next()) {
		auto pperms = problems_get_permissions(powner, ProblemType(ptype));
		// Give access to the problem's solutions' jobs to the problem's admin
		if (SubmissionType(stype) == SubmissionType::PROBLEM_SOLUTION and
			bool(uint(pperms & ProblemPermissions::EDIT)))
		{
			return PERM::VIEW | PERM::DOWNLOAD_REPORT;
		}
	}

	return PERM::NONE;
}

void Sim::jobs_handle() {
	STACK_UNWINDING_MARK;
	using PERM = JobPermissions;

	if (not session_open())
		return redirect("/login?" + request.target);

	StringView next_arg = url_args.extractNextArg();
	if (isDigit(next_arg)) {
		jobs_jid = next_arg;

		page_template(concat("Job ", jobs_jid), "body{padding-left:32px}");
		append("<script>preview_job(false, ", jobs_jid, ");</script>");
		return;
	}

	// Get permissions to overall job queue
	jobs_perms = jobs_get_permissions();

	InplaceBuff<32> query_suffix {};
	if (next_arg == "my")
		query_suffix.append("/u", session_user_id);
	else if (next_arg.size())
		return error404();
	else if (uint(~jobs_perms & PERM::VIEW_ALL))
		return error403();

	/* List jobs */
	page_template("Job queue", "body{padding-left:20px}");

	append("<h1>Jobs</h1>"
		"<script>"
			"tab_jobs_lister($('body'));"
		"</script>");
}
