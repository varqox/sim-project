#include "src/web_server/old/sim.hh"

namespace web_server::old {

void Sim::contests_handle() {
    STACK_UNWINDING_MARK;

    StringView next_arg = url_args.extract_next_arg();
    if (not next_arg.empty() and is_digit(next_arg.substr(1))) {
        if (next_arg[0] == 'c') {
            return contests_contest(next_arg.substr(1));
        }
        if (next_arg[0] == 'r') {
            return contests_contest_round(next_arg.substr(1));
        }
        if (next_arg[0] == 'p') {
            return contests_contest_problem(next_arg.substr(1));
        }
    }

    if (next_arg == "add") { // Add contest
        page_template("Add contest");
        append("add_contest(false, window.location.hash);");

    } else if (next_arg.empty()) { // List contests
        page_template("Contests");
        append("contest_chooser(false, window.location.hash);");

    } else {
        return error404();
    }
}

void Sim::contests_contest(StringView contest_id) {
    STACK_UNWINDING_MARK;

    StringView next_arg = url_args.extract_next_arg();
    if (next_arg.empty()) {
        page_template(intentional_unsafe_string_view(concat("Contest ", contest_id)));
        append("view_contest(false, ", contest_id, ", window.location.hash);");

    } else if (next_arg == "edit") {
        page_template(intentional_unsafe_string_view(concat("Edit contest ", contest_id)));
        append("edit_contest(false, ", contest_id, ", window.location.hash);");

    } else if (next_arg == "delete") {
        page_template(intentional_unsafe_string_view(concat("Delete contest ", contest_id)));
        append("delete_contest(false, ", contest_id, ", window.location.hash);");

    } else if (next_arg == "add_round") {
        page_template(intentional_unsafe_string_view(concat("Add round ", contest_id)));
        append("add_contest_round(false, ", contest_id, ", window.location.hash);");

    } else if (next_arg == "contest_user") {
        StringView user_id = url_args.extract_next_arg();
        if (user_id == "add") {
            page_template("Add contest user");
            append("add_contest_user(false, ", contest_id, ", window.location.hash);");
        } else if (is_digit(user_id)) {
            next_arg = url_args.extract_next_arg();
            if (next_arg == "change_mode") {
                page_template("Change contest user mode");
                append(
                    "change_contest_user_mode(false, ", contest_id, ",", user_id,
                    ", window.location.hash);");

            } else if (next_arg == "expel") {
                page_template("Expel user from the contest");
                append(
                    "expel_contest_user(false, ", contest_id, ",", user_id,
                    ", window.location.hash);");

            } else {
                error404();
            }
        } else {
            error404();
        }

    } else if (next_arg == "files") {
        StringView arg = url_args.extract_next_arg();
        if (arg == "add") {
            page_template("Add contest file");
            append("add_contest_file(false, ", contest_id, ", window.location.hash);");
        } else {
            error404();
        }

    } else {
        return error404();
    }
}

void Sim::contests_contest_round(StringView contest_round_id) {
    STACK_UNWINDING_MARK;

    StringView next_arg = url_args.extract_next_arg();
    if (next_arg.empty()) {
        page_template(intentional_unsafe_string_view(concat("Round ", contest_round_id)));
        append("view_contest_round(false, ", contest_round_id, ", window.location.hash);");

    } else if (next_arg == "edit") {
        page_template(intentional_unsafe_string_view(concat("Edit round ", contest_round_id)));
        append("edit_contest_round(false, ", contest_round_id, ", window.location.hash);");

    } else if (next_arg == "delete") {
        page_template(
            intentional_unsafe_string_view(concat("Delete round ", contest_round_id)));
        append("delete_contest_round(false, ", contest_round_id, ", window.location.hash);");

    } else if (next_arg == "attach_problem") {
        page_template(
            intentional_unsafe_string_view(concat("Attach problem ", contest_round_id)));
        append("add_contest_problem(false, ", contest_round_id, ", window.location.hash);");

    } else {
        return error404();
    }
}

void Sim::contests_contest_problem(StringView contest_problem_id) {
    STACK_UNWINDING_MARK;

    StringView next_arg = url_args.extract_next_arg();
    if (next_arg.empty()) {
        page_template(
            intentional_unsafe_string_view(concat("Contest problem ", contest_problem_id)));
        append("view_contest_problem(false, ", contest_problem_id, ", window.location.hash);");

    } else if (next_arg == "edit") {
        page_template(intentional_unsafe_string_view(
            concat("Edit contest problem ", contest_problem_id)));
        append("edit_contest_problem(false, ", contest_problem_id, ", window.location.hash);");

    } else if (next_arg == "submit") {
        page_template(
            intentional_unsafe_string_view(concat("Submit a solution ", contest_problem_id)));
        append(
            "add_contest_submission(false, undefined, undefined, {id:", contest_problem_id,
            "});");

    } else if (next_arg == "delete") {
        page_template(intentional_unsafe_string_view(
            concat("Delete contest problem ", contest_problem_id)));
        append(
            "delete_contest_problem(false, ", contest_problem_id, ", window.location.hash);");

    } else {
        return error404();
    }
}

} // namespace web_server::old
