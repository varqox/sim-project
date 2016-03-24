#include "errors.h"

void Errors::errorTemplate(const StringView& status, const char* code,
	const char* message)
{
	resp.status_code = status;
	resp.headers.clear();

	std::string prev = req->headers.get("Referer");
	if (prev.empty())
		prev = '/';

	auto ender = baseTemplate(status);
	append("<center>\n"
		"<h1 style=\"font-size:25px;font-weight:normal;\">", code, " &mdash; ",
			message, "</h1>\n"
		"<a class=\"btn\" href=\"", prev, "\">Go back</a>\n"
		"</center>");
}
