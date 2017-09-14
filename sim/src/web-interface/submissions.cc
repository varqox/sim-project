#include "sim.h"

void Sim::submissions_handle() {
	STACK_UNWINDING_MARK;

	StringView next_arg = url_args.extractNextArg();
	// Preview submissions
	if (isDigit(next_arg)) {
		page_template(concat("Submission ", next_arg));
		append("<script>preview_submission(false, ", next_arg, ");</script>");

	// List submissions
	} else if (next_arg.empty() and
		uint(submissions_get_permissions() & SubmissionPermissions::VIEW_ALL))
	{
		page_template("Submissions", "body{padding-left:32px}");
		append("<h1>Submissions</h1>"
			"<script>"
				"$(document).ready(function(){"
				"	var main = $('body');"
				"	tabmenu(function(x) { x.appendTo(main); }, ["
				"		'All submissions', function() {"
				"			console.log($(this).parent().next());"
				"			$(this).parent().next().remove();"
				"			main.append($('<div>'));"
				"			tab_submissions_lister(main.children().last(), '',"
				"				true);"
				"		},"
				"		'My submissions', function() {"
				"			console.log($(this).parent().next());"
				"			$(this).parent().next().remove();"
				"			main.append($('<div>'));"
				"			tab_submissions_lister(main.children().last(),"
				"				'/u' + logged_user_id());"
				"		}"
				"	]);"
				"});"
			"</script>");

	} else
		return error404();
}
