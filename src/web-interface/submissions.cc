#include "sim.h"

void Sim::submission_handle() {
	STACK_UNWINDING_MARK;

	if (not session_open())
		return redirect("/login?" + request.target);

	StringView submission_id = url_args.extractNextArg();
	if (not isDigit(submission_id))
		return error404();

	page_template(concat("Submission ", submission_id), "body{padding-left:32px}");
	append("<script>preview_submission(false, ", submission_id, ");</script>");
}
