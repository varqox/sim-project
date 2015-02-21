#pragma once

#include <map>
#include <string>

namespace server {
class HttpHeaders : public std::map<std::string, std::string> {
public:
	std::string& operator[](const std::string& n) {
		return std::map<std::string, std::string>::operator[](n);
	}
};
} // namespace server
