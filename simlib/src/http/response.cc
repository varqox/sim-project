#include "../../include/string.h"

using std::string;

namespace http {

string quote(StringView str) {
	string res;
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
