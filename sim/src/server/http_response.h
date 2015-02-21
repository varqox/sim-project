#pragma once

#include "http_headers.h"

namespace server {
class HttpResponse {
public:
	enum ContentType { TEXT, FILE } content_type;
	HttpHeaders headers, cookies;
	std::string content;

	HttpResponse(ContentType con_type = TEXT) : content_type(con_type),
			headers(), content() {}

	void setCookie(const std::string& name, const std::string& val,
			time_t expire = -1, const std::string& path = "", const std::string& domain ="",
			bool http_only = false, bool secure = false);

	std::string getCookie(const std::string& name) {
		std::string &cookie = cookies[name];
		return cookie.substr(0, cookie.find(';'));
	}
};
} // namespace server
