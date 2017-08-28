#include "../include/debug.h"
#include "../include/logger.h"
#include "../include/memory.h"
#include "../include/parsers.h"
#include "../include/process.h"
#include "../include/utilities.h"

#include <dirent.h>

using std::array;
using std::string;
using std::vector;

InplaceBuff<PATH_MAX> getCWD() {
	InplaceBuff<PATH_MAX> res;
	char* x = get_current_dir_name();
	if (!x)
		THROW("Failed to get CWD", error(errno));

	auto x_guard = make_call_in_destructor([x]{ free(x); });

	if (x[0] != '/') {
		errno = ENOENT;
		THROW("Failed to get CWD", error(errno));
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
	if ((rc == -1 && errno == ENAMETOOLONG)
		|| rc >= static_cast<int>(buff.size()))
	{
		array<char, 65536> buff2;
		rc = readlink(path.c_str(), buff2.data(), buff2.size());
		if (rc == -1 || rc >= static_cast<int>(buff2.size()))
			THROW("Failed: readlink('", path, "')", error(errno));

		return string(buff2.data(), rc);

	} else if (rc == -1)
		THROW("Failed: readlink('", path, "')", error(errno));

	return string(buff.data(), rc);
}

vector<pid_t> findProcessesByExec(vector<string> exec_set, bool include_me) {
	if (exec_set.empty())
		return {};

	pid_t pid, my_pid = (include_me ? -1 : getpid());
	DIR *dir = opendir("/proc");
	if (dir == nullptr)
		THROW("Cannot open /proc directory", error(errno));

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
	ssize_t buff_size = 0;
	for (int i = 0, n = exec_set.size(); i < n; ++i) {
		string deleted = concat_tostr(exec_set[i], " (deleted)");
		buff_size = meta::max(buff_size, (ssize_t)deleted.size());
		exec_set.emplace_back(std::move(deleted));
	}

	sort(exec_set); // To make binary search possible
	++buff_size; // For a terminating null character

	dirent *file;
	vector<pid_t> res;
	while ((file = readdir(dir)) != nullptr) {
		if (!isDigit(file->d_name))
			continue; // Not a process

		pid = atoi(file->d_name);
		if (pid == my_pid)
			continue; // Do not need to check myself

		// Process exe_path (/proc/pid/exe)
		string exe_path = concat_tostr("/proc/", file->d_name, "/exe");

		char buff[buff_size];
		ssize_t len = readlink(exe_path.c_str(), buff, buff_size);
		if (len == -1 || len >= buff_size)
			continue; // Error or name too long

		buff[len] = '\0';

		if (binary_search(exec_set, StringView{buff}))
			res.emplace_back(pid); // We have a match
	}

	closedir(dir);
	return res;
}

string chdirToExecDir() {
	string exec = getExec(getpid());
	// Erase file component
	size_t slash = exec.rfind('/');
	if (slash < exec.size())
		exec.resize(slash); // Erase filename

	if (chdir(exec.c_str()))
		THROW("chdir('", exec, "')", error(errno));

	return exec;
}

int8_t detectArchitecture(pid_t pid) {
	auto filename = concat("/proc/", pid, "/exe");
	FileDescriptor fd(filename.to_cstr(), O_RDONLY | O_LARGEFILE);
	if (fd == -1)
		THROW("open('", filename, "')", error(errno));

	// Read fourth byte and detect whether 32 or 64 bit
	unsigned char c;
	if (pread(fd, &c, 1, 4) != 1)
		THROW("pread()", error(errno));

	if (c == 1)
		return ARCH_i386;
	if (c == 2)
		return ARCH_x86_64;

	THROW("Unsupported architecture");
}

string getProcStat(pid_t pid, uint field_no) {
	string contents = getFileContents(concat("/proc/", pid, "/stat").to_cstr());
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
