#include "sim.h"


Sim::ProblemPermissions Sim::problems_get_permissions(StringView owner_id,
	ProblemType ptype)
{
	STACK_UNWINDING_MARK;
	using PERM = ProblemPermissions;

	static_assert(uint(PERM::NONE) == 0, "Needed below");
	constexpr PERM PERM_PROBLEM_ADMIN = PERM::VIEW_STATEMENT | PERM::VIEW_TAGS |
		PERM::VIEW_HIDDEN_TAGS | PERM::VIEW_SOLUTIONS_AND_SUBMISSIONS |
		PERM::VIEW_SIMFILE | PERM::VIEW_OWNER | PERM::VIEW_ADD_TIME |
		PERM::VIEW_RELATED_JOBS | PERM::DOWNLOAD | PERM::SUBMIT | PERM::EDIT |
		PERM::REUPLOAD | PERM::REJUDGE_ALL | PERM::EDIT_TAGS |
		PERM::EDIT_HIDDEN_TAGS | PERM::DELETE;
	constexpr PERM PERM_TEACHER = PERM::ADD | PERM::VIEW_TPUBLIC |
		PERM::VIEW_TCONTEST_ONLY | PERM::SELECT_BY_OWNER;

	if (not session_open())
		return (ptype == ProblemType::PUBLIC
			? PERM::VIEW_TPUBLIC | PERM::VIEW | PERM::VIEW_TAGS
			: PERM::VIEW_TPUBLIC);

	// Session is open
	if (session_user_type == UserType::ADMIN)
		return PERM::ADMIN | PERM::ADD | PERM::VIEW_TPUBLIC |
			PERM::VIEW_TCONTEST_ONLY | PERM::SELECT_BY_OWNER |
			PERM_PROBLEM_ADMIN;

	if (session_user_id == owner_id) {
		if (session_user_type == UserType::TEACHER)
			return PERM_TEACHER | PERM_PROBLEM_ADMIN;

		return PERM::VIEW_TPUBLIC | PERM_PROBLEM_ADMIN;
	}

	if (session_user_type == UserType::TEACHER) {
		if (ptype == ProblemType::PUBLIC)
			return PERM_TEACHER | PERM::VIEW_STATEMENT |
				PERM::VIEW_SOLUTIONS_AND_SUBMISSIONS | PERM::VIEW_TAGS |
				PERM::VIEW_SOLUTIONS_AND_SUBMISSIONS | PERM::VIEW_SIMFILE |
				PERM::VIEW_OWNER | PERM::VIEW_ADD_TIME | PERM::DOWNLOAD |
				PERM::SUBMIT;

		else if (ptype == ProblemType::CONTEST_ONLY)
			return PERM_TEACHER | PERM::VIEW_STATEMENT | PERM::VIEW_TAGS |
				PERM::VIEW_SIMFILE | PERM::VIEW_OWNER | PERM::VIEW_ADD_TIME;

		return PERM_TEACHER;
	}

	return (ptype == ProblemType::PUBLIC
		? PERM::VIEW_TPUBLIC | PERM::VIEW | PERM::VIEW_TAGS | PERM::SUBMIT
		: PERM::VIEW_TPUBLIC);
}

void Sim::problems_handle() {
	STACK_UNWINDING_MARK;

	StringView next_arg = url_args.extractNextArg();
	if (isDigit(next_arg)) {
		auto pid = next_arg;

		page_template(concat("Problem ", pid), "body{padding-left:32px}");
		append("<script>preview_problem(false, ", pid, ");</script>");
		return;
	}

	// Get permissions to overall problem set
	problems_perms = problems_get_permissions();

	InplaceBuff<32> query_suffix {};
	if (next_arg == "my")
		query_suffix.append("/u", session_user_id);
	else if (next_arg.size())
		return error404();

	/* List problems */
	page_template("Problems", "body{padding-left:20px}");

	append("<h1>Problems</h1>"
		"<script>"
			"tab_problems_lister($('body'));"
		"</script>");
}