#include "../include/process.h"
#include "../include/sandbox.h"

#include <gtest/gtest.h>
#include <iomanip>
#include <linux/version.h>
#include <sys/syscall.h>

inline std::ostream& operator<<(std::ostream& os, timespec ts) {
	return os << '{' << ts.tv_sec << '.' << std::setw(9) << ts.tv_nsec << '}';
}

TEST (Sandbox, run) {
	Sandbox sandbox;

	constexpr size_t MEM_LIMIT = 16 << 20; // 16 MB (in bytes)
	// Big RT limit is needed for tests where memory dump is created - it is really slow)
	constexpr timespec REAL_TIME_LIMIT = {3, 0}; // 3.0 s
	constexpr timespec CPU_TIME_LIMIT = {0, (int)0.2e9}; // 0.2 s

	Sandbox::Options opts {
		-1,
		-1,
		-1,
		REAL_TIME_LIMIT,
		MEM_LIMIT,
		CPU_TIME_LIMIT
	};

	// Compiled test case executable
	InplaceBuff<40> exec("/tmp/simlib.test.sandbox.XXXXXX");
	{
		exec.to_cstr(); // Add trailing '\0'
		int fd = mkstemp(exec.data());
		if (fd == -1)
			THROW("mkstemp()", errmsg());

		sclose(fd);
	}
	FileRemover exec_remover(exec.to_cstr());

	const auto test_cases_dir =
		concat(getExecDir(getpid()), "sandbox_test_cases/");

	auto compile_test_case = [&] (StringView case_filename, auto&&... cc_flags) {
		Spawner::ExitStat es = Spawner::run("cc", {
			"cc",
			"-O2",
			concat_tostr(test_cases_dir, case_filename),
			"-o",
			exec.to_string(),
			"-static",
			std::forward<decltype(cc_flags)>(cc_flags)...
		}, {-1, STDOUT_FILENO, STDERR_FILENO});

		// Compilation must be successful
		throw_assert(es.si.code == CLD_EXITED and es.si.status == 0);
	};

	Sandbox::ExitStat es;

	auto killed_or_dumped_by_abort = [&](decltype(es.si.code) si_code,
		const decltype(es.message)& message)
	{
		return ((si_code == CLD_KILLED and
				message == "killed by signal 6 - Aborted") or
			(si_code == CLD_DUMPED and
				message == "killed and dumped by signal 6 - Aborted"));
	};

	auto killed_or_dumped_by_segv = [&](decltype(es.si.code) si_code,
		const decltype(es.message)& message)
	{
		return ((si_code == CLD_KILLED and
				message == "killed by signal 11 - Segmentation fault") or
			(si_code == CLD_DUMPED and
				message == "killed and dumped by signal 11 - Segmentation fault"));
	};

	uint test_case = 0;
	stdlog.label(false);

	compile_test_case("1.c");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_EQ(es.si.code, CLD_KILLED);
	EXPECT_EQ(es.si.status, SIGKILL);
	EXPECT_EQ(es.message, "Memory limit exceeded");
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(0, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);

	compile_test_case("2.c");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_EQ(es.si.code, CLD_KILLED);
	EXPECT_EQ(es.si.status, SIGSEGV);
	EXPECT_EQ(es.message, "killed by signal 11 - Segmentation fault");
	EXPECT_EQ(es.cpu_runtime, (timespec{0, 0}));
	EXPECT_EQ(es.runtime, (timespec{0, 0}));
	EXPECT_EQ(es.vm_peak, 0);

	compile_test_case("3.c");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_EQ(es.si.code, CLD_EXITED);
	EXPECT_EQ(es.si.status, 37);
	EXPECT_EQ(es.message, "exited with 37");
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(0, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);

	compile_test_case("4.c");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_EQ(es.si.code, CLD_KILLED);
	EXPECT_EQ(es.si.status, SIGKILL);
	EXPECT_EQ(es.message, concat_tostr("forbidden syscall: ", SYS_socket, " - socket"));
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(0, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);

	compile_test_case("5.c");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_PRED2(killed_or_dumped_by_abort, es.si.code, es.message);
	EXPECT_EQ(es.si.status, SIGABRT);
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(0, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);

	compile_test_case("6.c");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_PRED2(killed_or_dumped_by_abort, es.si.code, es.message);
	EXPECT_EQ(es.si.status, SIGABRT);
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(0, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);

	// compile_test_case("6.c"); // not needed
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts, {{"/tmp", OpenAccess::RDONLY}});
	EXPECT_EQ(es.si.code, CLD_EXITED);
	EXPECT_EQ(es.si.status, 0);
	EXPECT_EQ(es.message, "");
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(0, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);

	for (auto perm : {OpenAccess::NONE, OpenAccess::WRONLY, OpenAccess::RDWR}) {
		// compile_test_case("6.c"); // not needed
		stdlog("Test case: ", test_case++);
		es = sandbox.run(exec, {}, opts, {{"/tmp", perm}});
		EXPECT_PRED2(killed_or_dumped_by_abort, es.si.code, es.message);
		EXPECT_EQ(es.si.status, SIGABRT);
		EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT((timespec{0, 0}), es.runtime);
		EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(0, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);
	}

	// Testing the allowing of lseek(), dup(), etc. on the closed stdin, stdout and stderr
	compile_test_case("7.c");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts, {{"/dev/null", OpenAccess::RDONLY}});
	EXPECT_EQ(es.si.code, CLD_EXITED);
	EXPECT_EQ(es.si.status, 0);
	EXPECT_EQ(es.message, "");
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(0, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);

	// Testing the use of readlink() as the marking of the end of initialization of glibc
	compile_test_case("8.c", "-m32");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_EQ(es.si.code, CLD_KILLED);
	EXPECT_EQ(es.si.status, SIGKILL);
	EXPECT_EQ(es.message, concat_tostr("forbidden syscall: uname"));
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(0, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);

	compile_test_case("8.c", "-m64");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_EQ(es.si.code, CLD_KILLED);
	EXPECT_EQ(es.si.status, SIGKILL);
	EXPECT_EQ(es.message, concat_tostr("forbidden syscall: uname"));
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(0, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);

	compile_test_case("9.c", "-m32");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_EQ(es.si.code, CLD_KILLED);
	EXPECT_EQ(es.si.status, SIGKILL);
	EXPECT_EQ(es.message, concat_tostr("forbidden syscall: set_thread_area"));
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(0, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);

	compile_test_case("9.c", "-m64");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_EQ(es.si.code, CLD_KILLED);
	EXPECT_EQ(es.si.status, SIGKILL);
	EXPECT_EQ(es.message, concat_tostr("forbidden syscall: 205 - set_thread_area"));
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(0, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);

	if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)) {
		compile_test_case("10.c", "-m32");
		stdlog("Test case: ", test_case++);
		es = sandbox.run(exec, {}, opts);
		EXPECT_EQ(es.si.code, CLD_KILLED);
		EXPECT_EQ(es.si.status, SIGKILL);
		EXPECT_EQ(es.message, concat_tostr("forbidden syscall: 384 - arch_prctl"));
		EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT((timespec{0, 0}), es.runtime);
		EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(0, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);
	}

	compile_test_case("10.c", "-m64");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_EQ(es.si.code, CLD_KILLED);
	EXPECT_EQ(es.si.status, SIGKILL);
	EXPECT_EQ(es.message, concat_tostr("forbidden syscall: arch_prctl"));
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(0, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);

	// Testing execve
	compile_test_case("11.c", "-m32");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_EQ(es.si.code, CLD_KILLED);
	EXPECT_EQ(es.si.status, SIGKILL);
	EXPECT_EQ(es.message, concat_tostr("forbidden syscall: execve"));
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(0, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);

	compile_test_case("11.c", "-m64");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_EQ(es.si.code, CLD_KILLED);
	EXPECT_EQ(es.si.status, SIGKILL);
	EXPECT_EQ(es.message, concat_tostr("forbidden syscall: execve"));
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(0, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);

	// Tests time-outing on time and memory vm_peak calculation on timeout
	compile_test_case("12.c");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_EQ(es.si.code, CLD_KILLED);
	EXPECT_EQ(es.si.status, SIGKILL);
	EXPECT_EQ(es.message, concat_tostr("killed by signal 9 - Killed"));
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	constexpr timespec CPU_TL_THRESHOLD = timespec{1, 0} - CPU_TIME_LIMIT;
	static_assert(CPU_TIME_LIMIT + CPU_TIME_LIMIT < CPU_TL_THRESHOLD,
		"Needed below to accurately check if the timeout occurred early enough");
	EXPECT_LE(es.cpu_runtime, CPU_TIME_LIMIT + CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LE(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(10 << 20, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);

	// Testing memory vm_peak calculation on normal exit
	compile_test_case("13.c");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_EQ(es.si.code, CLD_EXITED);
	EXPECT_EQ(es.si.status, 0);
	EXPECT_EQ(es.message, concat_tostr(""));
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LE(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(10 << 20, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);

	// Testing memory vm_peak calculation on abort
	compile_test_case("14.c");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_PRED2(killed_or_dumped_by_abort, es.si.code, es.message);
	EXPECT_EQ(es.si.status, SIGABRT);
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LE(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(10 << 20, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);

	// Testing memory vm_peak calculation on forbidden syscall
	compile_test_case("15.c");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_EQ(es.si.code, CLD_KILLED);
	EXPECT_EQ(es.si.status, SIGKILL);
	EXPECT_EQ(es.message, concat_tostr("forbidden syscall: ", SYS_socket, " - socket"));
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LE(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(10 << 20, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);

	// Testing memory vm_peak calculation on forbidden syscall (according to its callback)
	compile_test_case("16.c");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_EQ(es.si.code, CLD_KILLED);
	EXPECT_EQ(es.si.status, SIGKILL);
	EXPECT_EQ(es.message, concat_tostr("forbidden syscall: execve"));
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LE(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(10 << 20, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);

	// Testing memory vm_peak calculation on stack overflow
	compile_test_case("17.c");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_PRED2(killed_or_dumped_by_segv, es.si.code, es.message);
	EXPECT_EQ(es.si.status, SIGSEGV);
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LE(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(10 << 20, es.vm_peak);
	EXPECT_LE(es.vm_peak, MEM_LIMIT);

	// Testing memory vm_peak calculation on exit with exit(2)
	compile_test_case("18.c");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_EQ(es.si.code, CLD_EXITED);
	EXPECT_EQ(es.si.status, 11);
	EXPECT_EQ(es.message, concat_tostr("exited with 11"));
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LE(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(10 << 20, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);

	// Testing memory vm_peak calculation on exit with exit_group(2)
	compile_test_case("19.c");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_EQ(es.si.code, CLD_EXITED);
	EXPECT_EQ(es.si.status, 55);
	EXPECT_EQ(es.message, concat_tostr("exited with 55"));
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LE(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(10 << 20, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);

	// Testing memory vm_peak calculation on "Memory limit exceeded"
	compile_test_case("20.c");
	stdlog("Test case: ", test_case++);
	es = sandbox.run(exec, {}, opts);
	EXPECT_EQ(es.si.code, CLD_KILLED);
	EXPECT_EQ(es.si.status, SIGKILL);
	EXPECT_EQ(es.message, "Memory limit exceeded");
	EXPECT_LT((timespec{0, 0}), es.cpu_runtime);
	EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
	EXPECT_LT((timespec{0, 0}), es.runtime);
	EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
	EXPECT_LT(10 << 20, es.vm_peak);
	EXPECT_LT(0, es.vm_peak);
	EXPECT_LT(es.vm_peak, MEM_LIMIT);
}
