#include "simlib/working_directory.hh"
#include "simlib/defer.hh"
#include "simlib/path.hh"
#include "simlib/process.hh"

using std::string;

InplaceBuff<PATH_MAX> get_cwd() {
	InplaceBuff<PATH_MAX> res;
	char* x = get_current_dir_name();
	if (!x) {
		THROW("Failed to get CWD", errmsg());
	}

	Defer x_guard([x] {
		free(x); // NOLINT(cppcoreguidelines-no-malloc)
	});

	if (x[0] != '/') {
		errno = ENOENT;
		THROW("Failed to get CWD", errmsg());
	}

	res.append(x);
	if (res.back() != '/') {
		res.append('/');
	}

	return res;
}

void chdir_to_executable_dirpath() {
	InplaceBuff<PATH_MAX> exec_dir{
	   path_dirpath(intentional_unsafe_string_view(executable_path(getpid())))};
	if (chdir(exec_dir.to_cstr().c_str())) {
		THROW("chdir('", exec_dir, "')", errmsg());
	}
}
