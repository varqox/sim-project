#include "../../include/http/response.hh"
#include "../../include/ctype.hh"

using std::string;

namespace http {

string quote(StringView str) {
	string res;
	res.reserve(str.size() + 10);
	res += '"';
	for (auto c : str) {
		if (is_print(c) && c != '"')
			res += c;
		else
			(res += '\\') += c;
	}

	return (res += '"');
}

} // namespace http
