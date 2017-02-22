#pragma once

#include "http_headers.h"

#include <simlib/time.h>

namespace server {

class HttpResponse {
public:
	enum ContentType : uint8_t { TEXT, FILE, FILE_TO_REMOVE } content_type;
	std::string status_code;
	HttpHeaders headers, cookies;
	std::string content;

	explicit HttpResponse(ContentType con_type = TEXT,
			const std::string& stat_code = "200 OK")
			: content_type(con_type), status_code(stat_code), headers(),
			cookies(), content() {}

	HttpResponse(const HttpResponse&) = default;
	HttpResponse(HttpResponse&&) noexcept = default;
	HttpResponse& operator=(const HttpResponse&) = default;
	HttpResponse& operator=(HttpResponse&&) = default;

	~HttpResponse() {}

	void setCookie(const std::string& name, const std::string& val,
			time_t expire = -1, const std::string& path = "",
			const std::string& domain ="", bool http_only = false,
			bool secure = false);

	void setCache(bool to_public, uint max_age) {
		headers["expires"] = date("%a, %d %b %Y %H:%M:%S GMT",
			time(nullptr) + max_age);
		headers["cache-control"] = concat((to_public ? "public" : "private"),
			"; must-revalidate; max-age=", toStr(max_age));
	}

	std::string getCookie(const std::string& name) {
		std::string &cookie = cookies[name];
		return cookie.substr(0, cookie.find(';'));
	}
};

} // namespace server
