#include "http_request.h"

#include <simlib/debug.h>
#include <unistd.h>

using std::string;

namespace server {

HttpRequest::Form::~Form() {
	files.for_each([](auto&& p) { unlink(p.second.c_str()); });
}

StringView HttpRequest::getCookie(StringView name) const noexcept {
	STACK_UNWINDING_MARK;

	auto it = headers.find("cookie");
	if (not it)
		return "";

	StringView cookie = it->second;

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
