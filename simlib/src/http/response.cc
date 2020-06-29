#include "simlib/http/response.hh"
#include "simlib/ctype.hh"

using std::string;

namespace http {

string quote(const StringView& str) {
	string res;
	res.reserve(str.size() + 10);
	res += '"';
	for (auto c : str) {
		if (is_print(c) && c != '"') {
			res += c;
		} else {
			(res += '\\') += c;
		}
	}

	return (res += '"');
}

} // namespace http
