#include "sim_template.h"

#define ERROR_TEMPLATE(code, errno, message)                                  \
	resp_.status_code = code;                                                 \
	resp_.headers.clear();                                                    \
                                                                              \
	std::string prev = req_->headers.get("Referer");                          \
	if (prev.empty())                                                         \
		prev = '/';                                                           \
                                                                              \
	Template templ(*this, code);                                              \
	templ << "<center>\n"                                                     \
		"<h1 style=\"font-size:25px;font-weight:normal;\">" errno " &mdash; " \
			message "</h1>\n"                                                 \
		"<a class=\"btn\" href=\"" << prev << "\">Go back</a>\n"              \
		"</center>";

void Sim::error403() {
	ERROR_TEMPLATE("403 Forbidden", "403",
		"Sorry, but you're not allowed to see anything here.")
}

void Sim::error404() {
	ERROR_TEMPLATE("404 Not Found", "404", "Page not found")
}

void Sim::error500() {
	ERROR_TEMPLATE("500 Internal Server Error", "500", "Internal Server Error")
}
