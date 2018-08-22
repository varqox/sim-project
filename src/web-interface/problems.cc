#include "sim.h"

Sim::ProblemPermissions Sim::problems_get_overall_permissions() noexcept {
	STACK_UNWINDING_MARK;
	using PERM = ProblemPermissions;

	if (not session_is_open)
		return PERM::VIEW_TPUBLIC;

	switch (session_user_type) {
	case UserType::ADMIN:
		return PERM::VIEW_ALL | PERM::ADD | PERM::VIEW_TPUBLIC |
			PERM::VIEW_TCONTEST_ONLY | PERM::SELECT_BY_OWNER;
	case UserType::TEACHER:
		return PERM::ADD | PERM::VIEW_TPUBLIC | PERM::VIEW_TCONTEST_ONLY |
			PERM::SELECT_BY_OWNER;
	case UserType::NORMAL:
		return PERM::VIEW_TPUBLIC;
	}
}

Sim::ProblemPermissions Sim::problems_get_permissions(StringView owner_id,
	ProblemType ptype) noexcept
{
	STACK_UNWINDING_MARK;
	using PERM = ProblemPermissions;

	static_assert(uint(PERM::NONE) == 0, "Needed below");
	constexpr PERM PERM_PROBLEM_ADMIN = PERM::VIEW_STATEMENT | PERM::VIEW_TAGS |
		PERM::VIEW_HIDDEN_TAGS | PERM::VIEW_SOLUTIONS_AND_SUBMISSIONS |
		PERM::VIEW_SIMFILE | PERM::VIEW_OWNER | PERM::VIEW_ADD_TIME |
		PERM::VIEW_RELATED_JOBS | PERM::DOWNLOAD | PERM::SUBMIT | PERM::EDIT |
		PERM::SUBMIT_IGNORED | PERM::REUPLOAD | PERM::REJUDGE_ALL |
		PERM::EDIT_TAGS | PERM::EDIT_HIDDEN_TAGS | PERM::DELETE;

	if (not session_is_open)
		return (ptype == ProblemType::PUBLIC ? PERM::VIEW | PERM::VIEW_TAGS
			: PERM::NONE) | problems_get_overall_permissions();

	// Session is open
	if (session_user_type == UserType::ADMIN or session_user_id == owner_id)
		return PERM_PROBLEM_ADMIN | problems_get_overall_permissions();

	if (session_user_type == UserType::TEACHER) {
		if (ptype == ProblemType::PUBLIC)
			return PERM::VIEW_STATEMENT| PERM::VIEW_TAGS |
				PERM::VIEW_SOLUTIONS_AND_SUBMISSIONS | PERM::VIEW_SIMFILE |
				PERM::VIEW_OWNER | PERM::VIEW_ADD_TIME | PERM::DOWNLOAD |
				PERM::SUBMIT | problems_get_overall_permissions();

		if (ptype == ProblemType::CONTEST_ONLY)
			return PERM::VIEW_STATEMENT | PERM::VIEW_TAGS | PERM::VIEW_SIMFILE |
				PERM::VIEW_OWNER | PERM::VIEW_ADD_TIME |
				problems_get_overall_permissions();

		return problems_get_overall_permissions();
	}

	return (ptype == ProblemType::PUBLIC ?
		PERM::VIEW | PERM::VIEW_TAGS | PERM::SUBMIT : PERM::NONE) |
		problems_get_overall_permissions();
}

void Sim::problems_handle() {
	STACK_UNWINDING_MARK;

	StringView next_arg = url_args.extractNextArg();
	if (isDigit(next_arg)) {
		problems_pid = next_arg;
		return problems_problem();
	}

	// Get the overall permissions to the problem set
	problems_perms = problems_get_overall_permissions();

	// Add problem
	if (next_arg == "add") {
		page_template("Add problem");
		append("<script>add_problem(false);</script>");

	// List problems
	} else if (next_arg.empty()) {
		page_template("Problems", "body{padding-left:20px}");
		append("<script>problem_chooser(false, window.location.hash"
			");</script>");

	} else
		return error404();
}

void Sim::problems_problem() {
	STACK_UNWINDING_MARK;

	StringView next_arg = url_args.extractNextArg();
	if (next_arg.empty()) {
		page_template(concat("Problem ", problems_pid),
			"body{padding-left:20px}");
		append("<script>view_problem(false, ", problems_pid, ","
			" window.location.hash);</script>");

	} else if (next_arg == "submit") {
		page_template(concat("Submit solution to the problem ", problems_pid));
		append("<script>add_problem_submission(false, {id:", problems_pid, "})</script>");

	} else if (next_arg == "edit") {
		page_template(concat("Edit problem ", problems_pid));
		append("<script>edit_problem(false, ", problems_pid, ","
			" window.location.hash);</script>");

	} else if (next_arg == "reupload") {
		page_template(concat("Reupload problem ", problems_pid));
		append("<script>reupload_problem(false, ", problems_pid, ");</script>");

	} else if (next_arg == "delete") {
		page_template(concat("Delete problem ", problems_pid));
		append("<script>delete_problem(false, ", problems_pid, ");</script>");

	} else
		return error404();
}
