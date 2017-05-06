#include "sim.h"

#include <sim/jobs.h>

using std::string;

Sim::JobPermissions Sim::jobs_get_permissions(StringView creator_id,
	JobQueueStatus job_status)
{
	STACK_UNWINDING_MARK;
	using PERM = JobPermissions;

	D(throw_assert(session_is_open);) // Session must be open to access jobs

	if (session_user_type == UTYPE_ADMIN)
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
			(session_user_type == UTYPE_TEACHER ? PERM::VIEW_ALL : PERM::NONE);

	return PERM::NONE;
}
/*
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

static constexpr const char* job_status_as_td(JobQueueStatus status) noexcept  {
	using S = JobQueueStatus;

	switch (status) {
	case S::PENDING: return "<td class=\"status\">Pending</td>";
	case S::IN_PROGRESS: return "<td class=\"status yellow\">In progress</td>";
	case S::DONE: return "<td class=\"status green\">Done</td>";
	case S::FAILED: return "<td class=\"status red\">Failed</td>";
	case S::CANCELED: return "<td class=\"status blue\">Cancelled</td>";
	}
	return "<td class=\"status\">Unknown</td>";
}*/

void Sim::jobs_handle() {
	STACK_UNWINDING_MARK;

	using PERM = JobPermissions;

	if (not session_open())
		return redirect("/login?" + request.target);

	StringView next_arg = url_args.extractNextArg();
	if (isDigit(next_arg)) {
		jobs_job_id = next_arg;

		page_template(concat("Job ", jobs_job_id), "body{margin-left:32px}");
		append("<script>preview_job(", jobs_job_id, ", $('body'));</script>");
		return;
	}

	// Get permissions to overall job queue
	jobs_perms = jobs_get_permissions("", JobQueueStatus::DONE);

	StringView my = next_arg;
	if (next_arg == "my")
		my = "/my";
	else if (next_arg.size())
		return error404();
	else if (uint(~jobs_perms & PERM::VIEW_ALL))
		return error403();

	/* List jobs */
	page_template("Job queue", "body{margin-left:20px}");

	append("<h1>", (my.empty() ? "All jobs" : "My jobs"), "</h1>"
		"<table class=\"jobs\"></table>"
		"<script>"
			"new Jobs('/api/jobs", my, "', $('.jobs')).monitor_scroll();"
		"</script>");
}
