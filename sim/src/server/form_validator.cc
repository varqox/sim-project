#include "form_validator.h"


using std::string;

bool FormValidator::validate(string& var, const string& name,
		const string& name_to_print, size_t max_size) {

	const std::map<string, string>& form = form_.other;
	__typeof(form.begin()) it = form.find(name);

	if (it == form.end()) {
		append(errors_) << "<p>Invalid " << htmlSpecialChars(name_to_print)
			<< "</p>\n";
		return false;

	} else if (it->second.size() > max_size) {
		append(errors_) << "<p>" << htmlSpecialChars(name_to_print)
			<< " cannot be longer than " << toString(max_size) << "</p>\n";
		return false;
	}

	var = it->second;
	return true;
}
