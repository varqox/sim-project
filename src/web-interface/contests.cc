#include "sim.h"

Sim::ContestPermissions Sim::contests_get_permissions(bool is_public,
	ContestUserMode cu_mode)
{
	STACK_UNWINDING_MARK;
	using PERM = ContestPermissions;
	using CUM = ContestUserMode;

	if (not session_open())
		return (is_public ? PERM::VIEW_PUBLIC : PERM::VIEW_PUBLIC | PERM::VIEW);

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
	if (isDigit(next_arg)) {
		contests_cid = next_arg;
		return contests_contest();
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
		append("<h1>Contests</h1>");

		if (uint(contests_perms & (PERM::ADD_PRIVATE | PERM::ADD_PUBLIC)))
			append("<div><a class=\"btn\" onclick=\"add_contest(true,",
					uint(contests_perms & PERM::ADD_PRIVATE), ",",
					uint(contests_perms & PERM::ADD_PUBLIC), ")\">"
				"Add contest</a><div>");

		append("<script>"
				"tab_contests_lister($('body'));"
			"</script>");

	} else
		return error404();
}

void Sim::contests_contest() {
	STACK_UNWINDING_MARK;

	StringView next_arg = url_args.extractNextArg();
	if (next_arg.empty()) {
		page_template(concat("Contest ", contests_cid),
			"body{padding-left:20px}");
		append("<script>preview_contest(false, ", contests_cid, ");</script>");

	} else
		return error404();
}
