#pragma once

#include <map>
#include <simlib/string.h>

namespace server {

class HttpHeaders final : public std::map<std::string, std::string> {
public:
	HttpHeaders() = default;

	HttpHeaders(const HttpHeaders&) = default;
	HttpHeaders(HttpHeaders&&) noexcept = default;
	HttpHeaders& operator=(const HttpHeaders&) = default;
	HttpHeaders& operator=(HttpHeaders&&) noexcept = default;

	std::string& operator[](const std::string& key) {
		return std::map<std::string, std::string>::operator[](tolower(key));
	}

	bool isEqualTo(StringView key, StringView val) const {
		auto it = find(tolower(key.to_string()));
		return (it != end() && it->second == val);
	}

	std::string get(const std::string& key) const {
		auto it = find(tolower(key));
		return it == end() ? "" : it->second;
	}

	~HttpHeaders() {}
};

} // namespace server
