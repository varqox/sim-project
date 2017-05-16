#include "sim.h"

#include <sim/jobs.h>

using std::string;

Sim::JobPermissions Sim::jobs_get_permissions(StringView creator_id,
	JobQueueStatus job_status)
{
	STACK_UNWINDING_MARK;
	using PERM = JobPermissions;

	D(throw_assert(session_is_open);) // Session must be open to access jobs

	if (session_user_type == UserType::ADMIN)
		switch (job_status) {
		case JobQueueStatus::PENDING:
		case JobQueueStatus::IN_PROGRESS:
			return PERM::VIEW | PERM::VIEW_ALL | PERM::CANCEL;

		case JobQueueStatus::FAILED:
		case JobQueueStatus::CANCELED:
			return PERM::VIEW | PERM::VIEW_ALL | PERM::RESTART;

		default:
			return PERM::VIEW | PERM::VIEW_ALL;
		}

	static_assert(uint(PERM::NONE) == 0, "Needed below");
	if (session_user_id == creator_id)
		return PERM::VIEW | (isIn(job_status,
			{JobQueueStatus::PENDING, JobQueueStatus::IN_PROGRESS}) ?
				PERM::CANCEL : PERM::NONE) |
			(session_user_type == UserType::TEACHER ? PERM::VIEW_ALL
				: PERM::NONE);

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

		page_template(concat("Job ", jobs_job_id), "body{margin-left:32px}");
		append("<script>preview_job(false, ", jobs_job_id, ");</script>");
		return;
	}

	// Get permissions to overall job queue
	jobs_perms = jobs_get_permissions("", JobQueueStatus::DONE);

	bool show_all_jobs = true;
	if (next_arg == "my")
		show_all_jobs = false;
	else if (next_arg.size())
		return error404();
	else if (uint(~jobs_perms & PERM::VIEW_ALL))
		return error403();

	/* List jobs */
	page_template("Job queue", "body{margin-left:20px}");

	append("<h1>", (show_all_jobs ? "All jobs" : "My jobs"), "</h1>"
		"<table class=\"jobs\"></table>"
		"<script>"
			"new JobsLister(", show_all_jobs, ", $('.jobs')).monitor_scroll();"
		"</script>");
}
