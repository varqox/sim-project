#include "../include/process.h"
#include "../include/debug.h"
#include "../include/logger.h"
#include "../include/memory.h"
#include "../include/parsers.h"
#include "../include/utilities.h"

#include <dirent.h>
#include <thread>

using std::array;
using std::not_fn;
using std::optional;
using std::pair;
using std::string;
using std::vector;
using std::chrono::duration;

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

template <class...>
constexpr bool always_false = false;

// TODO: remove it
// Converts whole @p str to @p T or returns std::nullopt on errors like value
// represented in @p str is too big or invalid
template <class T, std::enable_if_t<not std::is_integral_v<std::remove_cv_t<
                                       std::remove_reference_t<T>>>,
                                    int> = 0>
std::optional<T> str2num(StringView str) noexcept {
#if 0 // TODO: use it when std::from_chars for double becomes implemented in
      // libstdc++ (also remove always false from the above and include
      // <cstdlib>)
	std::optional<T> res {std::in_place};
	auto [ptr, ec] = std::from_chars(str.begin(), str.end(), &*res);
	if (ptr != str.end() or ec != std::errc())
		return std::nullopt;

	return res;
#else
	static_assert(std::is_floating_point_v<T>);
	if (str.empty() or isspace(str[0]))
		return std::nullopt;

	try {
		InplaceBuff<4096> buff {str};
		CStringView cstr = buff.to_cstr();
		auto err = std::exchange(errno, 0);

		char* ptr;
		T res = [&] {
			if constexpr (std::is_same_v<T, float>)
				return ::strtof(cstr.data(), &ptr);
			else if constexpr (std::is_same_v<T, double>)
				return ::strtod(cstr.data(), &ptr);
			else if constexpr (std::is_same_v<T, long double>)
				return ::strtold(cstr.data(), &ptr);
			else
				static_assert(always_false<T>, "Cannot convert this type");
		}();

		std::swap(err, errno);
		if (err or ptr != cstr.end())
			return std::nullopt;

		return res;

	} catch (...) {
		return std::nullopt;
	}

#endif
}

void kill_processes_by_exec(vector<string> exec_set,
                            optional<duration<double>> wait_timeout,
                            bool kill_after_waiting, int terminate_signal) {
	STACK_UNWINDING_MARK;
	// TODO: change every getProcStat() to open file by FileDescriptor and don't
	// fail if the file does not exists -- it means that the process is already
	// dead
	constexpr uint START_TIME_FID =
	   21; // Number of the field in /proc/[pid]/stat that contains process
	       // start time

	using Victim = pair<pid_t, string>;
	vector<Victim> victims;
	for (pid_t pid : findProcessesByExec(std::move(exec_set))) {
		try {
			victims.emplace_back(pid, getProcStat(pid, START_TIME_FID));
		} catch (...) {
			// Ignore if process already died
			if (kill(pid, 0) == 0 or errno != ESRCH)
				throw;
		}
	}

	if (victims.empty())
		return;

	using std::chrono_literals::operator""s;

	auto ticks_per_second = sysconf(_SC_CLK_TCK);
	if (ticks_per_second == -1)
		THROW("sysconf(_SC_CLK_TCK) failed");

	StringView max_victim_start_time;
	for (auto& [pid, start_time] : victims) {
		if (start_time > max_victim_start_time)
			max_victim_start_time = start_time;
	}

	auto proc_uptime = getFileContents("/proc/uptime");
	auto current_uptime = toString(
	   str2num<double>(StringView(proc_uptime).extractLeading(not_fn(isspace)))
	         .value() *
	      ticks_per_second,
	   0);

	// If one of the processes has just appeared, wait for a clock tick
	// to distinguish it from a new process that may appear just after killing
	// with the same pid
	if (max_victim_start_time >= proc_uptime)
		std::this_thread::sleep_for(1s / ticks_per_second);

	// First try terminate_signal
	for (size_t i = 0; i < victims.size(); ++i) {
		auto pid = victims[i].first;
		if (kill(pid, terminate_signal) == 0)
			continue;

		if (errno != ESRCH)
			THROW("kill(", pid, ")", errmsg());

		swap(victims[i--], victims.back());
		victims.pop_back();
	}

	// Wait for victims to terminate
	auto wait_start_time = std::chrono::system_clock::now();
	auto curr_time = wait_start_time;
	while (curr_time < wait_start_time) {
		// Remove dead victims
		for (size_t i = 0; i < victims.size(); ++i) {
			auto& [pid, start_time] = victims[i];
			try {
				if (getProcStat(pid, START_TIME_FID) == start_time)
					continue; // Still alive
			} catch (...) {
				// Exception means that the process is dead
			}

			swap(victims[i--], victims.back());
			victims.pop_back();
		}

		if (victims.empty())
			break;

		curr_time = std::chrono::system_clock::now();
		auto sleep_interval = 0.04s;
		if (wait_timeout.has_value()) {
			auto remaining_wait = *wait_timeout - (curr_time - wait_start_time);
			if (remaining_wait <= 0s)
				break;

			if (remaining_wait < sleep_interval)
				sleep_interval = remaining_wait;
		}

		std::this_thread::sleep_for(sleep_interval);
	}

	// Kill remaining victims
	if (kill_after_waiting) {
		for (auto& [pid, start_time] : victims)
			(void)kill(pid, SIGKILL);
	}
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
