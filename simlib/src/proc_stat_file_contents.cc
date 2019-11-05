#include "../include/proc_stat_file_contents.hh"
#include "../include/filesystem.hh"
#include "../include/simple_parser.hh"

using std::string;
using std::vector;

ProcStatFileContents::ProcStatFileContents(string stat_file_contents)
   : contents_(std::move(stat_file_contents)) {
	SimpleParser sp(contents_);

	// [0] - Process pid
	fields_.emplace_back(sp.extract_next_non_empty(isspace));
	// [1] - Executable filename
	sp.extract_next_non_empty('(');
	sp.remove_leading('(');
	fields_.emplace_back(sp.extract_prefix(sp.rfind(')')));
	sp.remove_leading('(');
	// [>1]
	for (;;) {
		auto val = sp.extract_next_non_empty();
		if (val.empty())
			break;

		fields_.emplace_back(val);
	}
}

ProcStatFileContents ProcStatFileContents::get(pid_t pid) {
	string contents = get_file_contents(concat("/proc/", pid, "/stat"));
	return ProcStatFileContents {contents};
}
