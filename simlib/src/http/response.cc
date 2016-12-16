#include "../../include/string.h"

namespace http {

std::string quote(const StringView& str) {
	std::string res;
	res.reserve(str.size() + 10);
	res += '"';
	for (char c : str) {
		if (isprint(c) && c != '"')
			res += c;
		else
			(res += '\\') += c;
	}

	return (res += '"');
}

} // namespace http
