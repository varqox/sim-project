#include "simlib/path.hh"
#include "simlib/concat_tostr.hh"
#include "simlib/string_traits.hh"

using std::string;

string path_absolute(StringView path, string curr_dir) {
	if (path.empty())
		return curr_dir;

	string curr_path = std::move(curr_dir);
	if (path.front() == '/')
		curr_path = '/';

	if (not has_suffix(curr_path, "/"))
		curr_path += '/';

	auto erase_last_component = [&curr_path] {
		// Remove trailing '/' to help trim last component
		if (has_suffix(curr_path, "/") and curr_path != "/")
			curr_path.pop_back();

		curr_path.resize(curr_path.size() - path_filename(curr_path).size());
	};

	auto process_component = [&](StringView component) {
		if (component == "..") {
			erase_last_component();
			return;
		}

		// Current path is a directory
		if (not has_suffix(curr_path, "/") and not curr_path.empty())
			curr_path += '/';

		if (component == ".")
			return;

		back_insert(curr_path, component);
	};

	for (size_t i = 0; i < path.size(); ++i) {
		if (path[i] == '/')
			continue;

		size_t next_slash_pos = std::min(path.find('/', i + 1), path.size());
		process_component(path.substring(i, next_slash_pos));
		i = next_slash_pos;
	}

	if (has_suffix(path, "/") and not has_suffix(curr_path, "/"))
		curr_path += '/'; // Add trimmed trailing slash

	return curr_path;
}
