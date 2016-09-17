#include "form_validator.h"

using std::string;

bool FormValidator::validate(string& var, const string& name,
	const string& name_to_print, size_t max_size)
{
	auto const& form = form_.other;
	auto it = form.find(name);
	if (it == form.end()) {
		back_insert(errors_, "<pre class=\"error\">Invalid ",
			htmlSpecialChars(name_to_print), "</pre>");
		return false;

	} else if (it->second.size() > max_size) {
		back_insert(errors_, "<pre class=\"error\">",
			htmlSpecialChars(name_to_print), " cannot be longer than ",
			toString(max_size), " bytes</pre>");
		return false;
	}

	var = it->second;
	return true;
}
