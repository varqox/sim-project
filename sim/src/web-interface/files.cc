#include "sim.h"

void Sim::file_handle() {
	STACK_UNWINDING_MARK;

	StringView file_id = url_args.extractNextArg();
	if (file_id.empty() or not isAlnum(file_id))
		return error404();

	StringView next_arg = url_args.extractNextArg();
	if (next_arg == "edit") {
		page_template(intentionalUnsafeStringView(
			concat("Edit file ", file_id)),
				"body{padding-left:20px}");
		append("<script>edit_file(false, '", file_id, "',"
			" window.location.hash);</script>");

	} else if (next_arg == "delete") {
		page_template(intentionalUnsafeStringView(
			concat("Delete file ", file_id)),
				"body{padding-left:20px}");
		append("<script>delete_file(false, '", file_id, "',"
			" window.location.hash);</script>");

	} else
		error404();
}
