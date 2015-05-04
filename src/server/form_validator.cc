#include "form_validator.h"

using std::string;

bool FormValidator::validate(string& var, const string& name,
		const string& name_to_print, size_t max_size) {

	const std::map<string, string>& form = form_.other;
	__typeof(form.begin()) it = form.find(name);

	if (it == form.end()) {
		errors_.append("<p>Invalid ").append(htmlSpecialChars(name_to_print))
			.append("</p>\n");
		return false;

	} else if (it->second.size() > max_size) {
		errors_.append("<p>").append(htmlSpecialChars(name_to_print))
			.append(" cannot be longer than ").append(toString(max_size))
			.append("</p>\n");
		return false;
	}

	var = it->second;
	return true;
}
