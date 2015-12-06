#include "form_validator.h"

using std::string;

bool FormValidator::validate(string& var, const string& name,
		const string& name_to_print, size_t max_size) {

	const std::map<string, string>& form = form_.other;
	auto it = form.find(name);
	if (it == form.end()) {
		append(errors_) << "<pre class=\"error\">Invalid "
			<< htmlSpecialChars(name_to_print) << "</pre>\n";
		return false;

	} else if (it->second.size() > max_size) {
		append(errors_) << "<pre class=\"error\">"
			<< htmlSpecialChars(name_to_print) << " cannot be longer than "
			<< toString(max_size) << "</pre>\n";
		return false;
	}

	var = it->second;
	return true;
}
