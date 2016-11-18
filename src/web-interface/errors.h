#pragma once

#include "template.h"

class Errors : virtual protected Template {
private:
	void errorTemplate(const StringView& status, const char* code,
		const char* message);

protected:
	Errors() = default;

	Errors(const Errors&) = delete;
	Errors(Errors&&) = delete;
	Errors& operator=(const Errors&) = delete;
	Errors& operator=(Errors&&) = delete;

	virtual ~Errors() = default;

	// 403
	void error403() {
		errorTemplate("403 Forbidden", "403",
			"Sorry, but you're not allowed to see anything here.");
	}

	// 404
	void error404() { errorTemplate("404 Not Found", "404", "Page not found"); }

	// 500
	void error500() {
		errorTemplate("500 Internal Server Error", "500",
			"Internal Server Error");
	}
};
