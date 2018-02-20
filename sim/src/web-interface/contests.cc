#include "sim.h"

Sim::ContestPermissions Sim::contests_get_permissions(bool is_public,
	ContestUserMode cu_mode)
{
	STACK_UNWINDING_MARK;
	using PERM = ContestPermissions;
	using CUM = ContestUserMode;

	if (not session_open())
		return (is_public ? PERM::VIEW_PUBLIC | PERM::VIEW : PERM::VIEW_PUBLIC);

	if (session_user_type == UserType::ADMIN)
		return PERM::VIEW_PUBLIC | PERM::VIEW_ALL | PERM::ADD_PRIVATE |
			PERM::ADD_PUBLIC | PERM::SELECT_BY_USER | PERM::VIEW |
			PERM::PARTICIPATE | PERM::ADMIN | PERM::MAKE_PUBLIC |
			PERM::EDIT_OWNERS | PERM::DELETE;

	PERM overall_perms = (session_user_type == UserType::TEACHER ?
		PERM::VIEW_PUBLIC | PERM::ADD_PRIVATE : PERM::VIEW_PUBLIC);

	switch (cu_mode) {
	case CUM::OWNER:
		return overall_perms | PERM::VIEW | PERM::PARTICIPATE | PERM::ADMIN |
			PERM::EDIT_OWNERS | PERM::DELETE;

	case CUM::MODERATOR:
		return overall_perms | PERM::VIEW | PERM::PARTICIPATE | PERM::ADMIN;

	case CUM::CONTESTANT:
		return overall_perms | PERM::VIEW | PERM::PARTICIPATE;

	case CUM::IS_NULL:
		if (is_public)
			return overall_perms | PERM::VIEW | PERM::PARTICIPATE;
		else
			return overall_perms;
	}

	return overall_perms;
}

void Sim::contests_handle() {
	STACK_UNWINDING_MARK;
	using PERM = ContestPermissions;

	StringView next_arg = url_args.extractNextArg();
	if (not next_arg.empty() and isDigit(next_arg.substr(1))) {
		if (next_arg[0] == 'c') {
			contests_cid = next_arg.substr(1);
			return contests_contest();

		} else if (next_arg[0] == 'r') {
			contests_crid = next_arg.substr(1);
			return contests_contest_round();

		} else if (next_arg[0] == 'p') {
			contests_cpid = next_arg.substr(1);
			return contests_contest_problem();
		}
	}

	// Get the overall permissions to the contest set
	contests_perms = contests_get_permissions();

	// Add contest
	if (next_arg == "add") {
		if (uint(~contests_perms & (PERM::ADD_PRIVATE | PERM::ADD_PUBLIC)))
			return error403();

		page_template("Add contest");
		append("<script>add_contest(false,",
			uint(contests_perms & PERM::ADD_PRIVATE), ",",
			uint(contests_perms & PERM::ADD_PUBLIC), ");</script>");

	// List contests
	} else if (next_arg.empty()) {
		page_template("Contests", "body{padding-left:20px}");
		append("<script>contest_chooser(false, window.location.hash)</script>");

	} else
		return error404();
}

void Sim::contests_contest() {
	STACK_UNWINDING_MARK;

	StringView next_arg = url_args.extractNextArg();
	if (next_arg.empty()) {
		page_template(concat("Contest ", contests_cid),
			"body{padding-left:20px}");
		append("<script>view_contest(false, ", contests_cid, ","
			" window.location.hash);</script>");

	} else if (next_arg == "edit") {
		page_template(concat("Edit contest ", contests_cid),
			"body{padding-left:20px}");
		append("<script>edit_contest(false, ", contests_cid, ","
			" window.location.hash);</script>");

	} else if (next_arg == "delete") {
		page_template(concat("Delete contest ", contests_cid),
			"body{padding-left:20px}");
		append("<script>delete_contest(false, ", contests_cid, ","
			" window.location.hash);</script>");

	} else if (next_arg == "add_round") {
		page_template(concat("Add round ", contests_cid),
			"body{padding-left:20px}");
		append("<script>add_contest_round(false, ", contests_cid, ","
			" window.location.hash);</script>");

	} else
		return error404();
}

void Sim::contests_contest_round() {
	STACK_UNWINDING_MARK;

	StringView next_arg = url_args.extractNextArg();
	if (next_arg.empty()) {
		page_template(concat("Round ", contests_crid),
			"body{padding-left:20px}");
		append("<script>view_contest_round(false, ", contests_crid, ","
			" window.location.hash);</script>");

	} else if (next_arg == "edit") {
		page_template(concat("Edit round ", contests_crid),
			"body{padding-left:20px}");
		append("<script>edit_contest_round(false, ", contests_crid, ","
			" window.location.hash);</script>");

	} else if (next_arg == "delete") {
		page_template(concat("Delete round ", contests_crid),
			"body{padding-left:20px}");
		append("<script>delete_contest_round(false, ", contests_crid, ","
			" window.location.hash);</script>");

	} else if (next_arg == "attach_problem") {
		page_template(concat("Attach problem ", contests_crid),
			"body{padding-left:20px}");
		append("<script>add_contest_problem(false, ", contests_crid, ","
			" window.location.hash);</script>");

	} else
		return error404();
}

void Sim::contests_contest_problem() {
	STACK_UNWINDING_MARK;

	StringView next_arg = url_args.extractNextArg();
	if (next_arg.empty()) {
		page_template(concat("Contest problem ", contests_cpid),
			"body{padding-left:20px}");
		append("<script>view_contest_problem(false, ", contests_cpid, ","
			" window.location.hash);</script>");

	} else if (next_arg == "edit") {
		page_template(concat("Edit contest problem ", contests_cpid),
			"body{padding-left:20px}");
		append("<script>edit_contest_problem(false, ", contests_cpid, ","
			" window.location.hash);</script>");

	// } else if (next_arg == "submit") {
	// 	page_template(concat("Submit a solution ", contests_cpid),
	// 		"body{padding-left:20px}");
	// 	append("<script>add_submission(false, ", contests_cpid, ","
	// 		" window.location.hash);</script>");

	} else if (next_arg == "delete") {
		page_template(concat("Delete contest problem ", contests_cpid),
			"body{padding-left:20px}");
		append("<script>delete_contest_problem(false, ", contests_cpid, ","
			" window.location.hash);</script>");

	} else
		return error404();
}
