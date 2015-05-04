#include "http_response.h"

using std::string;

namespace server {

void HttpResponse::setCookie(const string& name, const string& val,
		time_t expire, const string& path, const string& domain, bool http_only,
		bool secure) {

	string value = val;

	if (expire != -1) {
		char buff[35];
		tm *ptm = gmtime(&expire);
		if (strftime(buff, 35, "%a, %d %b %Y %H:%M:%S GMT", ptm))
			value.append("; Expires=").append(buff);
	}

	if (!path.empty())
		value.append("; Path=").append(path);

	if (!domain.empty())
		value.append("; Domain=").append(domain);

	if (http_only)
		value.append("; HttpOnly");

	if (secure)
		value.append("; Secure");

	cookies[name] = value;
}

} // namespace server
