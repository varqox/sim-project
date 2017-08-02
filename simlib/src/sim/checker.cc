#include "../../include/debug.h"

#include <unistd.h>

using std::string;

namespace sim {

string obtainCheckerOutput(int fd, size_t max_length) {
	string res(max_length, '\0');

	size_t pos = 0;
	ssize_t k;
	do {
		k = pread(fd, const_cast<char*>(res.data()) + pos, max_length - pos,
				pos);
		if (k > 0) {
			pos += k;
		} else if (k == 0) {
			// We have read whole checker output
			res.resize(pos);
			// Remove trailing white characters
			while (res.size() and isspace(res.back()))
				res.pop_back();

			return res;

		} else if (errno != EINTR)
			THROW("read()", error(errno));

	} while (pos < max_length);

	// Checker output is not shorter than max_length

	// Replace last 3 characters with "..."
	if (max_length >= 3)
		res[max_length - 3] = res[max_length - 2] = res[max_length - 1] = '.';

	return res;
}

} // namespace sim
