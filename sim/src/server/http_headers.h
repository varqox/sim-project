#pragma once

#include <map>
#include <string>
#include "../include/string.h"

namespace server {
class HttpHeaders : public std::map<std::string, std::string> {
public:
	std::string& operator[](const std::string& n) {
		return std::map<std::string, std::string>::operator[](tolower(n));
	}
};
} // namespace server
