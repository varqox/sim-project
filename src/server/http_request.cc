#include "http_request.h"

#include <unistd.h>

using std::map;
using std::string;

namespace server {

HttpRequest::Form::~Form() {
	for (map<string, string>::iterator i = files.begin(); i != files.end(); ++i)
		unlink(i->second.c_str());
}

string HttpRequest::getCookie(const string& name) const {
	HttpHeaders::const_iterator it = headers.find("cookie");

	if (it == headers.end())
		return "";

	const string &cookie = it->second;

	for (size_t beg = 0; beg < cookie.size();) {
		if (cookie[beg] == ' ' && beg + 1 < cookie.size())
			++beg;
		if (0 == cookie.compare(beg, name.size(), name) &&
				beg + name.size() < cookie.size() &&
				cookie[beg + name.size()] == '=') {
			beg += name.size() + 1;
			size_t next = std::min(cookie.size(), find(cookie, ';', beg));
			return cookie.substr(beg, next - beg);
		}
		beg = find(cookie, ';', beg);
		if (beg < cookie.size())
			++beg;
	}
	return "";
}

} // namespace server
