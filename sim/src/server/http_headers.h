#pragma once

#include "../simlib/include/string.h"

#include <map>

namespace server {

class HttpHeaders : public std::map<std::string, std::string> {
public:
	std::string& operator[](const std::string& key) {
		return std::map<std::string, std::string>::operator[](tolower(key));
	}

	bool isEqualTo(const std::string& key, const std::string& val) const {
		const_iterator it = find(tolower(key));
		return (it != end() && it->second == val);
	}

	std::string get(const std::string& key) const {
		const_iterator it = find(tolower(key));
		return it == end() ? std::string() : it->second;
	}
};

} // namespace server
