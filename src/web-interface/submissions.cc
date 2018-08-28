#include "sim.h"

Sim::SubmissionPermissions Sim::submissions_get_overall_permissions() noexcept {
	using PERM = SubmissionPermissions;

	if (not session_is_open)
		return PERM::NONE;

	switch (session_user_type) {
	case UserType::ADMIN:
		return PERM::VIEW_ALL;
	case UserType::TEACHER:
	case UserType::NORMAL:
		return PERM::NONE;
	}
}

Sim::SubmissionPermissions Sim::submissions_get_permissions(
	StringView submission_owner, SubmissionType stype,
	Optional<ContestUserMode> cu_mode, StringView problem_owner) noexcept
{
	using PERM = SubmissionPermissions;
	using STYPE = SubmissionType;
	using CUM = ContestUserMode;

	PERM overall_perms = submissions_get_overall_permissions();

	if (not session_is_open)
		return PERM::NONE;

	static_assert((uint)PERM::NONE == 0, "Needed below");
	PERM PERM_SUBMISSION_ADMIN = PERM::VIEW | PERM::VIEW_SOURCE |
		PERM::VIEW_FINAL_REPORT | PERM::VIEW_RELATED_JOBS | PERM::REJUDGE |
		(isIn(stype, {STYPE::NORMAL, STYPE::IGNORED})
			? PERM::CHANGE_TYPE | PERM::DELETE : PERM::NONE);

	if (session_user_type == UserType::ADMIN or
		(cu_mode.has_value() and
			isOneOf(cu_mode.value(), CUM::MODERATOR, CUM::OWNER)))
	{
		return overall_perms | PERM_SUBMISSION_ADMIN;
	}

	// This check has to be done as the last one because it gives the least
	// permissions
	if (session_user_id == problem_owner) {
		if (stype == STYPE::PROBLEM_SOLUTION)
			return overall_perms | PERM::VIEW | PERM::VIEW_SOURCE |
				PERM::VIEW_FINAL_REPORT | PERM::VIEW_RELATED_JOBS |
				PERM::REJUDGE;

		if (session_user_id == submission_owner)
			return overall_perms | PERM_SUBMISSION_ADMIN;

		return overall_perms | PERM::VIEW | PERM::VIEW_SOURCE |
			PERM::VIEW_FINAL_REPORT | PERM::VIEW_RELATED_JOBS | PERM::REJUDGE;
	}

	if (session_user_id == submission_owner)
		return overall_perms | PERM::VIEW | PERM::VIEW_SOURCE;

	return overall_perms;
}

void Sim::submissions_handle() {
	STACK_UNWINDING_MARK;

	StringView next_arg = url_args.extractNextArg();
	// View submission
	if (isDigit(next_arg)) {
		page_template(concat("Submission ", next_arg),
			"body{padding-left:20px}");
		append("<script>view_submission(false, ", next_arg, ","
			" window.location.hash);</script>");

	// List submissions
	} else if (next_arg.empty()) {
		page_template("Submissions", "body{padding-left:20px}");
		append("<h1>Submissions</h1>"
			"<script>"
				"$(document).ready(function(){"
					"var main = $('body');"
					"tabmenu(function(x) { x.appendTo(main); }, ["
						"'All submissions', function() {"
							"main.children('.tabmenu + div, .loader,"
								".loader-info').remove();"
							"tab_submissions_lister($('<div>').appendTo(main),"
								"'', true);"
						"},"
						"'My submissions', function() {"
							"main.children('.tabmenu + div, .loader,"
								".loader-info').remove();"
							"tab_submissions_lister($('<div>').appendTo(main),"
								"'/u' + logged_user_id());"
						"}"
					"]);"
				"});"
			"</script>");

	} else
		return error404();
}
