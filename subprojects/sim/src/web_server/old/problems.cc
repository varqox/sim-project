#include "sim.hh"

#include <sim/problems/permissions.hh>

namespace web_server::old {

void Sim::problems_handle() {
    STACK_UNWINDING_MARK;

    StringView next_arg = url_args.extract_next_arg();
    if (is_digit(next_arg)) {
        problems_pid = next_arg;
        return problems_problem();
    }

    if (next_arg == "add") { // Add problem
        page_template("Add problem");
        append("add_problem(false);");
    } else {
        return error404();
    }
}

void Sim::problems_problem() {
    STACK_UNWINDING_MARK;

    StringView next_arg = url_args.extract_next_arg();
    if (next_arg.empty()) {
        page_template(from_unsafe{concat("Problem ", problems_pid)});
        append("view_problem(false, ", problems_pid, ", window.location.hash);");

    } else if (next_arg == "submit") {
        page_template(from_unsafe{concat("Submit solution to the problem ", problems_pid)});
        append("add_problem_submission(false, {id:", problems_pid, "})");

    } else if (next_arg == "edit") {
        page_template(from_unsafe{concat("Edit problem ", problems_pid)});
        append("edit_problem(false, ", problems_pid, ", window.location.hash);");

    } else if (next_arg == "reupload") {
        page_template(from_unsafe{concat("Reupload problem ", problems_pid)});
        append("reupload_problem(false, ", problems_pid, ");");

    } else if (next_arg == "reset_time_limits") {
        page_template(from_unsafe{concat("Reset problem time limits ", problems_pid)});
        append("reset_problem_time_limits(false, ", problems_pid, ");");

    } else if (next_arg == "delete") {
        page_template(from_unsafe{concat("Delete problem ", problems_pid)});
        append("delete_problem(false, ", problems_pid, ");");

    } else if (next_arg == "merge") {
        page_template(from_unsafe{concat("Merge problem ", problems_pid)});
        append("merge_problem(false, ", problems_pid, ");");

    } else {
        return error404();
    }
}

} // namespace web_server::old
