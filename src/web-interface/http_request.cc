#include "http_request.h"

#include <simlib/debug.h>
#include <unistd.h>

using std::map;
using std::string;

namespace server {

HttpRequest::Form::~Form() {
	for (auto&& p : files)
		unlink(p.second.c_str());
}

string HttpRequest::getCookie(const string& name) const {
	STACK_UNWINDING_MARK;

	auto it = headers.find("cookie");

	if (it == headers.end())
		return "";

	const string &cookie = it->second;

	for (size_t beg = 0; beg < cookie.size();) {
		if (cookie[beg] == ' ' && beg + 1 < cookie.size())
			++beg;

		if (0 == cookie.compare(beg, name.size(), name) &&
			beg + name.size() < cookie.size() &&
			cookie[beg + name.size()] == '=')
		{
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
