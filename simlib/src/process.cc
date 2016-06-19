#include "../include/memory.h"
#include "../include/process.h"

#include <dirent.h>

using std::array;
using std::string;
using std::unique_ptr;
using std::vector;

int Spawner::NormalTimer::cpid;
std::mutex Spawner::NormalTimer::lock;

string Spawner::receiveErrorMessage(int status, int fd) {
	string message;
	array<char, 4096> buff;
	// Read errors from fd
	ssize_t rc;
	while ((rc = read(fd, buff.data(), buff.size())) > 0)
		message.append(buff.data(), rc);

	if (message.size()) // Error in tracee
		THROW(message);

	if (WIFEXITED(status))
		message = concat("returned ",
			toString(WEXITSTATUS(status)));

	else if (WIFSIGNALED(status))
		message = concat("killed by signal ", toString(WTERMSIG(status)), " - ",
			strsignal(WTERMSIG(status)));

	return message;
};

string getCWD() {
	array<char, PATH_MAX> buff;
	if (!getcwd(buff.data(), buff.size()))
		THROW("Failed to get CWD", error(errno));

	if (buff[0] != '/') {
		errno = ENOENT; // Improper path, but getcwd() succeed
		THROW("Failed to get CWD", error(errno));
	}

	string res(buff.data());
	if (res.back() != '/')
		res += '/';

	return res;
}

string getExec(pid_t pid) {
	array<char, 4096> buff;
	string path = concat("/proc/", toString(pid), "/exe");

	ssize_t rc = readlink(path.c_str(), buff.data(), buff.size());
	if ((rc == -1 && errno == ENAMETOOLONG)
		|| rc >= static_cast<int>(buff.size()))
	{
		array<char, 65536> buff2;
		rc = readlink(path.c_str(), buff2.data(), buff2.size());
		if (rc == -1 || rc >= static_cast<int>(buff2.size()))
			THROW("Failed: readlink(", path, ')', error(errno));

		return string(buff2.data(), rc);

	} else if (rc == -1)
		THROW("Failed: readlink(", path, ')', error(errno));

	return string(buff.data(), rc);
}

vector<pid_t> findProcessesByExec(string exec, bool include_me) {
	if (exec.empty())
		return {};

	if (exec.front() != '/')
		exec = concat(getCWD(), exec);
	// Make exec absolute
	exec = abspath(exec);

	pid_t pid, my_pid = (include_me ? -1 : getpid());
	DIR *dir = opendir("/proc");
	if (dir == nullptr)
		THROW("Cannot open /proc directory", error(errno));

	// Process with deleted exec will have " (deleted)" suffix in result of
	// readlink(2)
	string exec_deleted = concat(exec, " (deleted)");
	ssize_t buff_size = exec_deleted.size() + 1; // +1 - for terminating null

	dirent *file;
	vector<pid_t> res;
	while ((file = readdir(dir)) != nullptr) {
		if (!isDigit(file->d_name))
			continue; // Not a process

		pid = atoi(file->d_name);
		if (pid == my_pid)
			continue; // Do not need to check myself

		// Process exe_path (/proc/pid/exe)
		string exe_path = concat("/proc/", file->d_name, "/exe");

		char buff[buff_size];
		ssize_t len = readlink(exe_path.c_str(), buff, buff_size);
		if (len == -1 || len >= buff_size)
			continue; // Error or name too long

		buff[len] = '\0';

		if (exec == buff || exec_deleted == buff)
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
		exec.erase(exec.begin() + slash + 1, exec.end()); // Erase filename

	if (chdir(exec.c_str()) == -1)
		THROW("chdir(", exec, ')', error(errno));

	return exec;
}

int8_t detectArchitecture(pid_t pid) {
	string filename = concat("/proc/", toString(pid), "/exe");

	int fd = open(filename.c_str(), O_RDONLY | O_LARGEFILE);
	if (fd == -1)
		THROW("open('", filename, "')", error(errno));

	Closer closer(fd);
	// Read fourth byte and detect if 32 or 64 bit
	unsigned char c;
	if (lseek(fd, 4, SEEK_SET) == (off_t)-1)
		THROW("lseek()", error(errno));

	if (read(fd, &c, 1) != 1)
		THROW("read()", error(errno));

	if (c == 1)
		return ARCH_i386;
	if (c == 2)
		return ARCH_x86_64;

	THROW("Unsupported architecture");
}
