#include "sim.h"

#include <sim/jobs.h>

using std::string;

Sim::JobPermissions Sim::jobs_get_permissions(StringView creator_id,
	JobType job_type, JobStatus job_status)
{
	STACK_UNWINDING_MARK;
	using PERM = JobPermissions;
	using JT = JobType;

	D(throw_assert(session_is_open);) // Session must be open to access jobs

	JobPermissions perm = (isIn(job_type, { JT::ADD_PROBLEM,
		JT::REUPLOAD_PROBLEM, JT::ADD_JUDGE_MODEL_SOLUTION,
		JT::REUPLOAD_JUDGE_MODEL_SOLUTION})
		? PERM::DOWNLOAD_REPORT | PERM::DOWNLOAD_UPLOADED_PACKAGE : PERM::NONE);

	if (session_user_type == UserType::ADMIN) {
		switch (job_status) {
		case JobStatus::PENDING:
		case JobStatus::IN_PROGRESS:
			return perm | PERM::VIEW | PERM::DOWNLOAD_REPORT | PERM::VIEW_ALL |
				PERM::CANCEL;

		case JobStatus::FAILED:
		case JobStatus::CANCELED:
			return perm | PERM::VIEW | PERM::DOWNLOAD_REPORT | PERM::VIEW_ALL |
				PERM::RESTART;

		default:
			return perm | PERM::VIEW | PERM::DOWNLOAD_REPORT | PERM::VIEW_ALL;
		}
	}

	static_assert(uint(PERM::NONE) == 0, "Needed below");
	if (session_user_id == creator_id)
		return perm | PERM::VIEW | (isIn(job_status,
			{JobStatus::PENDING, JobStatus::IN_PROGRESS}) ?
				PERM::CANCEL : PERM::NONE);

	return PERM::NONE;
}

void Sim::jobs_handle() {
	STACK_UNWINDING_MARK;
	using PERM = JobPermissions;

	if (not session_open())
		return redirect("/login?" + request.target);

	StringView next_arg = url_args.extractNextArg();
	if (isDigit(next_arg)) {
		jobs_job_id = next_arg;

		page_template(concat("Job ", jobs_job_id), "body{padding-left:32px}");
		append("<script>preview_job(false, ", jobs_job_id, ");</script>");
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

	append("<h1>", (query_suffix.size == 0 ? "All jobs" : "My jobs"), "</h1>"
		"<table class=\"jobs\"></table>"
		"<script>"
			"new JobsLister($('.jobs'),'", query_suffix, "').monitor_scroll();"
		"</script>");
}
