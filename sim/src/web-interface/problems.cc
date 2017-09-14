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
		PERM::SUBMIT_IGNORED | PERM::REUPLOAD | PERM::REJUDGE_ALL |
		PERM::EDIT_TAGS | PERM::EDIT_HIDDEN_TAGS | PERM::DELETE;
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
	using PERM = ProblemPermissions;

	StringView next_arg = url_args.extractNextArg();
	if (isDigit(next_arg)) {
		problems_pid = next_arg;
		return problems_problem();
	}

	// Get the overall permissions to the problem set
	problems_perms = problems_get_permissions();

	// Add problem
	if (next_arg == "add") {
		if (uint(~problems_perms & PERM::ADD))
			return error403();

		page_template("Add problem");
		append("<script>add_problem(false);</script>");

	// List problems
	} else if (next_arg.empty()) {
		page_template("Problems", "body{padding-left:20px}");
		append("<h1>Problems</h1>");

		if (uint(problems_perms & PERM::ADD))
			append("<div><a class=\"btn\" onclick=\"add_problem(true)\">"
				"Add problem</a><div>");

		append("<script>"
				"tab_problems_lister($('body'));"
			"</script>");

	} else
		return error404();
}

void Sim::problems_problem() {
	STACK_UNWINDING_MARK;

	StringView next_arg = url_args.extractNextArg();
	if (next_arg.empty()) {
		page_template(concat("Problem ", problems_pid));
		append("<script>preview_problem(false, ", problems_pid, ");</script>");

	} else if (next_arg == "submit") {
		InplaceBuff<32> owner;
		uint type = uint(ProblemType::VOID);
		InplaceBuff<PROBLEM_NAME_MAX_LEN> name;
		auto stmt = mysql.prepare("SELECT owner, type, name FROM problems"
			" WHERE id=?");
		stmt.bindAndExecute(problems_pid);
		stmt.res_bind_all(owner, type, name);
		if (not stmt.next())
			return error404();

		problems_perms = problems_get_permissions(owner, ProblemType(type));

		page_template(concat("Submit solution to the problem ", problems_pid));
		append("<script>add_submission(false, ", problems_pid, ", ",
			jsonStringify(name),
			(uint(problems_perms & ProblemPermissions::SUBMIT_IGNORED) ?
				", true" : ", false"), ")</script>");

	} else if (next_arg == "solutions") {
		page_template(concat("Problem ", problems_pid, " solutions"));
		append("<script>preview_problem(false, ", problems_pid,
			", 'solutions');</script>");

	} else if (next_arg == "edit") {
		page_template(concat("Edit problem ", problems_pid));
		append("TODO");

	} else if (next_arg == "reupload") {
		page_template(concat("Reupload problem ", problems_pid));
		append("<script>reupload_problem(false, ", problems_pid, ");</script>");

	} else if (next_arg == "delete") {
		page_template(concat("Delete problem ", problems_pid));
		append("TODO");

	} else
		return error404();
	return;
}