#include "../include/proc_stat_file_contents.hh"
#include "../include/file_contents.hh"
#include "../include/string_traits.hh"

#include <cctype>
#include <functional>

using std::string;
using std::vector;

ProcStatFileContents::ProcStatFileContents(string stat_file_contents)
   : contents_(std::move(stat_file_contents)) {
	StringView str(contents_);

	// [0] - Process pid
	str.extract_leading(isspace);
	fields_.emplace_back(str.extract_leading(std::not_fn(isspace)));
	// [1] - Executable filename
	str.remove_leading([](int c) { return c != '('; });
	assert(has_prefix(str, "("));
	str.remove_prefix(1);
	fields_.emplace_back(str.extract_prefix(str.rfind(')')));
	assert(has_prefix(str, ")"));
	str.remove_prefix(1);
	// [>1]
	for (;;) {
		str.remove_leading(isspace);
		auto val = str.extract_leading(std::not_fn(isspace));
		if (val.empty())
			break;

		fields_.emplace_back(val);
	}
}

ProcStatFileContents ProcStatFileContents::get(pid_t pid) {
	string contents = get_file_contents(concat("/proc/", pid, "/stat"));
	return ProcStatFileContents {contents};
}
