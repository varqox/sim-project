#pragma once

#include <map>
#include <simlib/string.h>

namespace server {

class HttpHeaders : public std::map<std::string, std::string> {
public:
	HttpHeaders() = default;

	std::string& operator[](const std::string& key) {
		return std::map<std::string, std::string>::operator[](tolower(key));
	}

	bool isEqualTo(const std::string& key, const std::string& val) const {
		const_iterator it = find(tolower(key));
		return (it != end() && it->second == val);
	}

	std::string get(const std::string& key) const {
		const_iterator it = find(tolower(key));
		return it == end() ? "" : it->second;
	}

	virtual ~HttpHeaders() {}
};

} // namespace server
