#pragma once

#include <simlib/avl_dict.h>

namespace server {

class HttpHeaders final
   : public AVLDictMap<std::string, std::string, LowerStrCompare> {
public:
	HttpHeaders() = default;

	HttpHeaders(const HttpHeaders&) = default;
	HttpHeaders(HttpHeaders&&) noexcept = default;
	HttpHeaders& operator=(const HttpHeaders&) = default;
	HttpHeaders& operator=(HttpHeaders&&) noexcept = default;

	std::string& operator[](std::string key) {
		return AVLDictMap::operator[](std::move(key));
	}

	bool isEqualTo(StringView key, StringView val) const noexcept {
		auto it = find(tolower(key.to_string()));
		return (it and it->second == val);
	}

	StringView get(StringView key) const noexcept {
		auto it = find(key);
		return (it ? it->second : StringView());
	}

	~HttpHeaders() {}
};

} // namespace server
