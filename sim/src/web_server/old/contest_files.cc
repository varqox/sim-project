#include "sim.hh"

namespace web_server::old {

void Sim::contest_file_handle() {
    STACK_UNWINDING_MARK;

    StringView contest_file_id = url_args.extract_next_arg();
    if (contest_file_id.empty() or not is_alnum(contest_file_id)) {
        return error404();
    }

    StringView next_arg = url_args.extract_next_arg();
    if (next_arg == "edit") {
        page_template(intentional_unsafe_string_view(concat("Edit contest file ", contest_file_id))
        );
        append("edit_contest_file(false, '", contest_file_id, "', window.location.hash);");

    } else if (next_arg == "delete") {
        page_template(intentional_unsafe_string_view(concat("Delete contest file ", contest_file_id)
        ));
        append("delete_contest_file(false, '", contest_file_id, "', window.location.hash);");

    } else {
        error404();
    }
}

} // namespace web_server::old
