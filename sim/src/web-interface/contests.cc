#include "sim.h"

Sim::ContestPermissions Sim::contests_get_overall_permissions() noexcept {
	STACK_UNWINDING_MARK;
	using PERM = ContestPermissions;

	if (not session_is_open)
		return PERM::VIEW_PUBLIC;

	switch (session_user_type) {
	case UserType::ADMIN:
		return PERM::VIEW_ALL | PERM::VIEW_PUBLIC | PERM::ADD_PRIVATE |
			PERM::ADD_PUBLIC;
	case UserType::TEACHER:
		return PERM::VIEW_PUBLIC | PERM::ADD_PRIVATE;
	case UserType::NORMAL:
		return PERM::VIEW_PUBLIC;
	}
}

Sim::ContestPermissions Sim::contests_get_permissions(bool is_public,
	Optional<ContestUserMode> cu_mode) noexcept
{
	STACK_UNWINDING_MARK;
	using PERM = ContestPermissions;
	using CUM = ContestUserMode;

	PERM overall_perms = contests_get_overall_permissions();

	if (not session_is_open) {
		if (is_public)
			return overall_perms | PERM::VIEW;
		else
			return overall_perms;
	}

	if (session_user_type == UserType::ADMIN)
		return overall_perms | PERM::VIEW | PERM::PARTICIPATE | PERM::ADMIN |
			PERM::DELETE | PERM::MAKE_PUBLIC | PERM::SELECT_FINAL_SUBMISSIONS |
			PERM::VIEW_ALL_CONTEST_SUBMISSIONS | PERM::MANAGE_CONTEST_ENTRY_TOKEN;

	if (not cu_mode.has_value()) {
		if (is_public)
			return overall_perms | PERM::VIEW | PERM::PARTICIPATE;
		else
			return overall_perms;
	}

	switch (cu_mode.value()) {
	case CUM::OWNER:
		return overall_perms | PERM::VIEW | PERM::PARTICIPATE | PERM::ADMIN |
			PERM::DELETE | PERM::SELECT_FINAL_SUBMISSIONS |
			PERM::VIEW_ALL_CONTEST_SUBMISSIONS | PERM::MANAGE_CONTEST_ENTRY_TOKEN;

	case CUM::MODERATOR:
		return overall_perms | PERM::VIEW | PERM::PARTICIPATE | PERM::ADMIN |
			PERM::SELECT_FINAL_SUBMISSIONS | PERM::VIEW_ALL_CONTEST_SUBMISSIONS |
			PERM::MANAGE_CONTEST_ENTRY_TOKEN;

	case CUM::CONTESTANT:
		return overall_perms | PERM::VIEW | PERM::PARTICIPATE;
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
	contests_perms = contests_get_overall_permissions();

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

	} else if (next_arg == "contest_user") {
		StringView user_id = url_args.extractNextArg();
		if (user_id == "add") {
			page_template(concat("Add contest user"),
				"body{padding-left:20px}");
			append("<script>add_contest_user(false, ", contests_cid,
				", window.location.hash);</script>");
		} else if (isDigit(user_id)) {
			next_arg = url_args.extractNextArg();
			if (next_arg == "change_mode") {
				page_template(concat("Change contest user mode"),
					"body{padding-left:20px}");
				append("<script>change_contest_user_mode(false, ", contests_cid,
					",", user_id, ", window.location.hash);</script>");

			} else if (next_arg == "expel") {
				page_template(concat("Expel user from the contest"),
					"body{padding-left:20px}");
				append("<script>expel_contest_user(false, ", contests_cid, ",",
					user_id, ", window.location.hash);</script>");

			} else
				error404();
		} else
			error404();

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

	} else if (next_arg == "submit") {
		page_template(concat("Submit a solution ", contests_cpid),
			"body{padding-left:20px}");
		append("<script>add_contest_submission(false, undefined, undefined, {id:", contests_cpid, "});</script>");

	} else if (next_arg == "delete") {
		page_template(concat("Delete contest problem ", contests_cpid),
			"body{padding-left:20px}");
		append("<script>delete_contest_problem(false, ", contests_cpid, ","
			" window.location.hash);</script>");

	} else
		return error404();
}

void Sim::enter_contest() {
	STACK_UNWINDING_MARK;

	page_template(concat("Enter contest"), "body{padding-left:20px}");
	append("<script>enter_contest_using_token(false, '",
		url_args.extractNextArg(), "');</script>");
}
