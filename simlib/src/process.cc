#include "../include/process.hh"
#include "../include/debug.hh"
#include "../include/directory.hh"
#include "../include/file_contents.hh"
#include "../include/path.hh"
#include "../include/proc_stat_file_contents.hh"
#include "../include/string_traits.hh"
#include "../include/string_transform.hh"
#include "../include/working_directory.hh"

#include <functional>
#include <thread>
#include <unistd.h>

using std::array;
using std::not_fn;
using std::optional;
using std::pair;
using std::string;
using std::vector;
using std::chrono::duration;

string executable_path(pid_t pid) {
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

vector<pid_t> find_processes_by_executable_path(vector<string> exec_set,
                                                bool include_me) {
	if (exec_set.empty())
		return {};

	pid_t pid, my_pid = (include_me ? -1 : getpid());
	Directory dir("/proc");
	if (dir == nullptr)
		THROW("Cannot open /proc directory", errmsg());

	decltype(get_cwd()) cwd;
	for (auto& exec : exec_set) {
		if (exec.front() != '/') {
			if (cwd.size == 0) // cwd is not set
				cwd = get_cwd();
			exec = concat_tostr(cwd, exec);
		}

		// Make exec absolute
		exec = path_absolute(exec);
	}

	// Process with deleted exec will have " (deleted)" suffix in result of
	// readlink(2)
	size_t buff_size = 0;
	for (int i = 0, n = exec_set.size(); i < n; ++i) {
		string deleted = concat_tostr(exec_set[i], " (deleted)");
		buff_size = std::max(buff_size, deleted.size());
		exec_set.emplace_back(std::move(deleted));
	}

	sort(exec_set); // To make binary search possible
	++buff_size; // For a terminating null character

	vector<pid_t> res;
	for_each_dir_component(dir, [&](dirent* file) {
		if (!is_digit(file->d_name))
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

void kill_processes_by_exec(vector<string> exec_set,
                            optional<duration<double>> wait_timeout,
                            bool kill_after_waiting, int terminate_signal) {
	STACK_UNWINDING_MARK;
	// TODO: change every ProcStatFileContents::get() to open file by
	// FileDescriptor and use ProcStatFileContents::from_proc_stat_contents()
	// and don't fail if the file does not exists -- it means that the process
	// is already dead
	constexpr uint START_TIME_FID =
	   21; // Number of the field in /proc/[pid]/stat that contains process
	       // start time

	using Victim = pair<pid_t, string>;
	vector<Victim> victims;
	for (pid_t pid : find_processes_by_executable_path(std::move(exec_set))) {
		try {
			victims.emplace_back(pid, ProcStatFileContents::get(pid)
			                             .field(START_TIME_FID)
			                             .to_string());
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

	auto proc_uptime = get_file_contents("/proc/uptime");
	auto current_uptime = to_string(
	   str2num<double>(StringView(proc_uptime).extract_leading(not_fn(isspace)))
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
				auto proc_stat = ProcStatFileContents::get(pid);
				if (proc_stat.field(START_TIME_FID) == start_time)
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

ArchKind detect_architecture(pid_t pid) {
	auto filename = concat("/proc/", pid, "/exe");
	FileDescriptor fd(filename, O_RDONLY | O_CLOEXEC);
	if (fd == -1)
		THROW("open('", filename, "')", errmsg());

	// Read fourth byte and detect whether 32 or 64 bit
	unsigned char c;
	if (pread(fd, &c, 1, 4) != 1)
		THROW("pread()", errmsg());

	if (c == 1)
		return ArchKind::i386;
	if (c == 2)
		return ArchKind::x86_64;

	THROW("Unsupported architecture");
}
