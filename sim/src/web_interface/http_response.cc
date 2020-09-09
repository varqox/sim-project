#include "http_response.hh"

#include <ctime>
#include <simlib/debug.hh>

using std::string;

namespace server {

void HttpResponse::set_cookie(const string& name, const string& val,
                              time_t expire, const string& path,
                              const string& domain, bool http_only,
                              bool secure) {
	STACK_UNWINDING_MARK;

	string value = val;

	if (expire != -1) {
		char buff[35];
		tm* ptm = gmtime(&expire);
		if (strftime(buff, 35, "%a, %d %b %Y %H:%M:%S GMT", ptm))
			value.append("; Expires=").append(buff);
	}

	if (path.size())
		back_insert(value, "; Path=", path);

	if (domain.size())
		back_insert(value, "; Domain=", domain);

	if (http_only)
		value.append("; HttpOnly");

	if (secure)
		value.append("; Secure");

	cookies[name] = value;
}

} // namespace server
