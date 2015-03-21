#pragma once

#include "http_request.h"

#include "../include/string.h"

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
			const std::string& name_to_print);

	bool validate(std::string& var, const std::string& name,
			const std::string& name_to_print, size_t max_size);

	// validate field + check it by comp
	template<class Checker>
	bool validate(std::string& var, const std::string& name,
			const std::string& name_to_print, Checker check,
			const std::string& error);

	template<class Checker>
	bool validate(std::string& var, const std::string& name,
			const std::string& name_to_print, Checker check, size_t max_size,
			const std::string& error);

	// Like validate but also validate not blank
	bool validateNotBlank(std::string& var, const std::string& name,
			const std::string& name_to_print);

	bool validateNotBlank(std::string& var, const std::string& name,
			const std::string& name_to_print, size_t max_size);

	// validate field + check it by comp
	template<class Checker>
	bool validateNotBlank(std::string& var, const std::string& name,
			const std::string& name_to_print, Checker check,
			const std::string& error);

	template<class Checker>
	bool validateNotBlank(std::string& var, const std::string& name,
			const std::string& name_to_print, Checker check, size_t max_size,
			const std::string& error);

	bool exist(const std::string& name) {
		return 0 != form_.other.count(name);
	}

	void error(const std::string& error) {
		errors_.append("<p>").append(htmlSpecialChars(error)).append("</p>\n");
	}

	std::string errors() const { return errors_; }

	bool noErrors() const { return errors_.empty(); }
};

// validate field + check it by comp
template<class Checker>
inline bool FormValidator::validate(std::string& var, const std::string& name,
		const std::string& name_to_print, Checker check,
		const std::string& error) {
	if (validate(var, name, name_to_print)) {
		if (check(var))
			return true;
		errors_.append("<p>").append(htmlSpecialChars(error))
			.append("</p>\n");
	}
	return false;
}

template<class Checker>
inline bool FormValidator::validate(std::string& var, const std::string& name,
		const std::string& name_to_print, Checker check, size_t max_size,
		const std::string& error) {
	if (validate(var, name, name_to_print, max_size)) {
		if (check(var))
			return true;
		errors_.append("<p>").append(htmlSpecialChars(error))
			.append("</p>\n");
	}
	return false;
}

// Like validate but also validate not blank
inline bool FormValidator::validateNotBlank(std::string& var, const std::string& name,
		const std::string& name_to_print) {
	if (validate(var, name, name_to_print) && var.empty()) {
		errors_.append("<p>").append(name_to_print)
			.append(" cannot be blank</p>\n");
		return false;
	}
	return true;
}

inline bool FormValidator::validateNotBlank(std::string& var, const std::string& name,
		const std::string& name_to_print, size_t max_size) {
	if (validate(var, name, name_to_print, max_size) && var.empty()) {
		errors_.append("<p>").append(name_to_print)
			.append(" cannot be blank</p>\n");
		return false;
	}
	return true;
}

// validate field + check it by comp
template<class Checker>
inline bool FormValidator::validateNotBlank(std::string& var, const std::string& name,
		const std::string& name_to_print, Checker check,
		const std::string& error) {
	if (validateNotBlank(var, name, name_to_print)) {
		if (check(var))
			return true;
		errors_.append("<p>").append(htmlSpecialChars(error))
			.append("</p>\n");
	}
	return false;
}

template<class Checker>
inline bool FormValidator::validateNotBlank(std::string& var, const std::string& name,
		const std::string& name_to_print, Checker check, size_t max_size,
		const std::string& error) {
	if (validateNotBlank(var, name, name_to_print, max_size)) {
		if (check(var))
			return true;
		errors_.append("<p>").append(htmlSpecialChars(error))
			.append("</p>\n");
	}
	return false;
}
