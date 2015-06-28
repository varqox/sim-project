#pragma once

#include "http_request.h"

#include "../include/string.h"
#include "../include/utility.h"

class FormValidator {
private:
	const server::HttpRequest::Form& form_;
	std::string errors_;

public:
	explicit FormValidator(const server::HttpRequest::Form& form)
			: form_(form), errors_() {}

	~FormValidator() {}

	// if valid returns true
	bool validate(std::string& var, const std::string& name,
			const std::string& name_to_print, size_t max_size = -1);

	// validate field + check it by comp
	template<class Checker>
	bool validate(std::string& var, const std::string& name,
			const std::string& name_to_print, Checker check,
			const std::string& error, size_t max_size = -1);

	// Like validate but also validate not blank
	bool validateNotBlank(std::string& var, const std::string& name,
			const std::string& name_to_print, size_t max_size = -1);

	// validate field + check it by comp
	template<class Checker>
	bool validateNotBlank(std::string& var, const std::string& name,
			const std::string& name_to_print, Checker check,
			const std::string& error, size_t max_size = -1);

	bool exist(const std::string& name) {
		return 0 != form_.other.count(name);
	}

	void addError(const std::string& error) {
		append(errors_) << "<p>" << htmlSpecialChars(error) << "</p>\n";
	}


	std::string getFilePath(const std::string& name) {
		const std::map<std::string, std::string>& form = form_.files;

		__typeof(form.begin()) it = form.find(name);
		return it == form.end() ? "" : it->second;
	}


	std::string errors() const { return errors_; }

	bool noErrors() const { return errors_.empty(); }
};

// validate field + check it by comp
template<class Checker>
inline bool FormValidator::validate(std::string& var, const std::string& name,
		const std::string& name_to_print, Checker check,
		const std::string& error, size_t max_size) {

	if (validate(var, name, name_to_print, max_size)) {
		if (check(var))
			return true;

		append(errors_) << "<p>" << htmlSpecialChars(error.empty() ?
				(name_to_print + " validation error") : error) << "</p>\n";
	}

	return false;
}

// Like validate but also validate not blank
inline bool FormValidator::validateNotBlank(std::string& var,
		const std::string& name, const std::string& name_to_print,
		size_t max_size) {

	if (validate(var, name, name_to_print, max_size) && var.empty()) {
		append(errors_) << "<p>" << name_to_print << " cannot be blank</p>\n";
		return false;
	}

	return true;
}

// validate field + check it by comp
template<class Checker>
inline bool FormValidator::validateNotBlank(std::string& var,
		const std::string& name, const std::string& name_to_print,
		Checker check, const std::string& error, size_t max_size) {

	if (validateNotBlank(var, name, name_to_print, max_size)) {
		if (check(var))
			return true;

		append(errors_) << "<p>" << htmlSpecialChars(error.empty() ?
				(name_to_print + " validation error") : error) << "</p>\n";
	}

	return false;
}
