#include "sim.hh"

using sim::ContestUser;
using sim::Problem;
using sim::User;

Sim::SubmissionPermissions Sim::submissions_get_overall_permissions() noexcept {
	using PERM = SubmissionPermissions;

	if (not session_is_open)
		return PERM::NONE;

	switch (session_user_type) {
	case User::Type::ADMIN: return PERM::VIEW_ALL;
	case User::Type::TEACHER:
	case User::Type::NORMAL: return PERM::NONE;
	}

	return PERM::NONE; // Shouldn't happen
}

Sim::SubmissionPermissions Sim::submissions_get_permissions(
   std::optional<uintmax_t> submission_owner, SubmissionType stype,
   std::optional<ContestUser::Mode> cu_mode,
   decltype(Problem::owner) problem_owner) noexcept {
	using PERM = SubmissionPermissions;
	using STYPE = SubmissionType;
	using CUM = ContestUser::Mode;

	PERM overall_perms = submissions_get_overall_permissions();

	if (not session_is_open)
		return PERM::NONE;

	static_assert((uint)PERM::NONE == 0, "Needed below");
	PERM PERM_SUBMISSION_ADMIN =
	   PERM::VIEW | PERM::VIEW_SOURCE | PERM::VIEW_FINAL_REPORT |
	   PERM::VIEW_RELATED_JOBS | PERM::REJUDGE |
	   (is_one_of(stype, STYPE::NORMAL, STYPE::IGNORED)
	       ? PERM::CHANGE_TYPE | PERM::DELETE
	       : PERM::NONE);

	if (session_user_type == User::Type::ADMIN or
	    (cu_mode.has_value() and
	     is_one_of(cu_mode.value(), CUM::MODERATOR, CUM::OWNER))) {
		return overall_perms | PERM_SUBMISSION_ADMIN;
	}

	// This check has to be done as the last one because it gives the least
	// permissions
	if (session_is_open and problem_owner and
	    WONT_THROW(str2num<uintmax_t>(session_user_id).value()) ==
	       problem_owner.value()) {
		if (stype == STYPE::PROBLEM_SOLUTION)
			return overall_perms | PERM::VIEW | PERM::VIEW_SOURCE |
			       PERM::VIEW_FINAL_REPORT | PERM::VIEW_RELATED_JOBS |
			       PERM::REJUDGE;

		if (submission_owner and
		    WONT_THROW(str2num<uintmax_t>(session_user_id).value()) ==
		       submission_owner.value()) {
			return overall_perms | PERM_SUBMISSION_ADMIN;
		}

		return overall_perms | PERM::VIEW | PERM::VIEW_SOURCE |
		       PERM::VIEW_FINAL_REPORT | PERM::VIEW_RELATED_JOBS |
		       PERM::REJUDGE;
	}

	if (session_is_open and submission_owner and
	    WONT_THROW(str2num<uintmax_t>(session_user_id).value()) ==
	       submission_owner.value()) {
		return overall_perms | PERM::VIEW | PERM::VIEW_SOURCE;
	}

	return overall_perms;
}

void Sim::submissions_handle() {
	STACK_UNWINDING_MARK;

	StringView next_arg = url_args.extract_next_arg();
	if (is_digit(next_arg)) { // View submission
		page_template(
		   intentional_unsafe_string_view(concat("Submission ", next_arg)),
		   "body{padding-left:20px}");
		append("<script>view_submission(false, ", next_arg,
		       ", window.location.hash);</script>");

	} else if (next_arg.empty()) { // List submissions
		page_template("Submissions", "body{padding-left:20px}");
		// clang-format off
		append("<h1>Submissions</h1>"
		       "<script>"
		       "$(document).ready(function(){"
		           "var main = $('body');"
		           "tabmenu(function(x) { x.appendTo(main); }, ["
		               "'All submissions', function() {"
		                   "main"
		                      ".children("
		                         "'.tabmenu + div, .loader,.loader-info')"
		                      ".remove();"
		                   "tab_submissions_lister($('<div>').appendTo(main),"
		                                          "'', true);"
		               "},"
		               "'My submissions', function() {"
		                   "main"
		                      ".children("
		                         "'.tabmenu + div, .loader,.loader-info')"
		                      ".remove();"
		                   "tab_submissions_lister($('<div>').appendTo(main),"
		                                          "'/u' + logged_user_id());"
		               "}"
		           "]);"
		       "});"
		       "</script>");
		// clang-format on

	} else {
		return error404();
	}
}
