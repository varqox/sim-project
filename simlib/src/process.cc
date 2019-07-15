#include "../include/process.h"
#include "../include/debug.h"
#include "../include/logger.h"
#include "../include/memory.h"
#include "../include/parsers.h"
#include "../include/utilities.h"

#include <dirent.h>

using std::array;
using std::string;
using std::vector;

InplaceBuff<PATH_MAX> getCWD() {
	InplaceBuff<PATH_MAX> res;
	char* x = get_current_dir_name();
	if (!x)
		THROW("Failed to get CWD", errmsg());

	CallInDtor x_guard([x] { free(x); });

	if (x[0] != '/') {
		errno = ENOENT;
		THROW("Failed to get CWD", errmsg());
	}

	res.append(x);
	if (res.back() != '/')
		res.append('/');

	return res;
}

string getExec(pid_t pid) {
	array<char, 4096> buff;
	string path = concat_tostr("/proc/", pid, "/exe");

	ssize_t rc = readlink(path.c_str(), buff.data(), buff.size());
	if ((rc == -1 && errno == ENAMETOOLONG) ||
	    rc >= static_cast<int>(buff.size())) {
		array<char, 65536> buff2;
		rc = readlink(path.c_str(), buff2.data(), buff2.size());
		if (rc == -1 || rc >= static_cast<int>(buff2.size()))
			THROW("Failed: readlink('", path, "')", errmsg());

		return string(buff2.data(), rc);

	} else if (rc == -1)
		THROW("Failed: readlink('", path, "')", errmsg());

	return string(buff.data(), rc);
}

vector<pid_t> findProcessesByExec(vector<string> exec_set, bool include_me) {
	if (exec_set.empty())
		return {};

	pid_t pid, my_pid = (include_me ? -1 : getpid());
	Directory dir("/proc");
	if (dir == nullptr)
		THROW("Cannot open /proc directory", errmsg());

	decltype(getCWD()) cwd;
	for (auto& exec : exec_set) {
		if (exec.front() != '/') {
			if (cwd.size == 0) // cwd is not set
				cwd = getCWD();
			exec = concat_tostr(cwd, exec);
		}

		// Make exec absolute
		exec = abspath(exec);
	}

	// Process with deleted exec will have " (deleted)" suffix in result of
	// readlink(2)
	size_t buff_size = 0;
	for (int i = 0, n = exec_set.size(); i < n; ++i) {
		string deleted = concat_tostr(exec_set[i], " (deleted)");
		buff_size = meta::max(buff_size, deleted.size());
		exec_set.emplace_back(std::move(deleted));
	}

	sort(exec_set); // To make binary search possible
	++buff_size; // For a terminating null character

	vector<pid_t> res;
	forEachDirComponent(dir, [&](dirent* file) {
		if (!isDigit(file->d_name))
			return; // Not a process

		pid = atoi(file->d_name);
		if (pid == my_pid)
			return; // Do not need to check myself

		// Process exe_path (/proc/pid/exe)
		string exe_path = concat_tostr("/proc/", file->d_name, "/exe");

		InplaceBuff<32> buff(buff_size);
		ssize_t len = readlink(exe_path.c_str(), buff.data(), buff_size);
		if (len == -1 || len >= (ssize_t)buff_size)
			return; // Error or name too long

		buff.size = len;
		buff[len] = '\0';

		if (binary_search(exec_set, StringView {buff}))
			res.emplace_back(pid); // We have a match
	});

	return res;
}

string getExecDir(pid_t pid) {
	string exec = getExec(pid);
	// Erase file component
	size_t slash = exec.rfind('/');
	if (slash < exec.size())
		exec.resize(slash + 1); // Erase filename

	return exec;
}

string chdirToExecDir() {
	string exec_dir = getExecDir(getpid());
	if (chdir(exec_dir.c_str()))
		THROW("chdir('", exec_dir, "')", errmsg());

	return exec_dir;
}

int8_t detectArchitecture(pid_t pid) {
	auto filename = concat("/proc/", pid, "/exe");
	FileDescriptor fd(filename, O_RDONLY | O_CLOEXEC);
	if (fd == -1)
		THROW("open('", filename, "')", errmsg());

	// Read fourth byte and detect whether 32 or 64 bit
	unsigned char c;
	if (pread(fd, &c, 1, 4) != 1)
		THROW("pread()", errmsg());

	if (c == 1)
		return ARCH_i386;
	if (c == 2)
		return ARCH_x86_64;

	THROW("Unsupported architecture");
}

string getProcStat(pid_t pid, uint field_no) {
	string contents = getFileContents(concat("/proc/", pid, "/stat"));
	SimpleParser sp {contents};

	// [0] - Process pid
	StringView val = sp.extractNextNonEmpty(isspace);
	if (field_no == 0)
		return val.to_string();

	// [1] Executable filename
	sp.removeLeading(isspace);
	sp.removeLeading('(');
	val = sp.extractPrefix(sp.rfind(')'));
	if (field_no == 1)
		return val.to_string();

	sp.removeLeading(')');

	// [2...]
	for (field_no -= 1; field_no > 0; --field_no)
		val = sp.extractNextNonEmpty(isspace);

	return val.to_string();
}
