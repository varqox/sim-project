#include <simlib/defer.hh>
#include <simlib/inplace_buff.hh>
#include <simlib/path.hh>
#include <simlib/process.hh>
#include <simlib/working_directory.hh>
#include <unistd.h>

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

void chdir_relative_to_executable_dirpath(StringView path) {
    if (path == ".") {
        path = "";
    }

    auto new_cwd = executable_path(getpid());
    new_cwd.resize(path_dirpath(new_cwd).size());
    new_cwd += path;
    if (chdir(new_cwd.data())) {
        THROW("chdir(", new_cwd, ')', errmsg());
    }
}
