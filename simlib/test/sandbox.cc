#include "simlib/sandbox.hh"
#include "compilation_cache.hh"
#include "simlib/concurrent/job_processor.hh"
#include "simlib/path.hh"
#include "simlib/process.hh"
#include "simlib/temporary_file.hh"

#include <chrono>
#include <gtest/gtest.h>
#include <iomanip>
#include <linux/version.h>
#include <sys/syscall.h>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;

using std::string;

class SandboxTests {
	static constexpr size_t MEM_LIMIT = 16 << 20; // 16 MiB (in bytes)
	// Big RT limit is needed for tests where memory dump is created - it is
	// really slow)
	static constexpr std::chrono::nanoseconds REAL_TIME_LIMIT = 3s;
	static constexpr std::chrono::nanoseconds CPU_TIME_LIMIT = 200ms;
	static constexpr Sandbox::Options SANDBOX_OPTIONS{
	   -1, -1, -1, REAL_TIME_LIMIT, MEM_LIMIT, CPU_TIME_LIMIT};

	const string& test_cases_dir_;
	TemporaryFile executable_{"/tmp/simlib.test.sandbox.XXXXXX"};

public:
	explicit SandboxTests(const string& test_cases_dir)
	: test_cases_dir_(test_cases_dir) {}

private:
	template <class... Flags>
	void compile_test_case(StringView test_case_filename, Flags&&... cc_flags) {
		CompilationCache ccache = {
		   "/tmp/simlib-sandbox-test-compilation-cache/",
		   std::chrono::hours(24)};
		auto path = concat_tostr(test_cases_dir_, test_case_filename);
		if (ccache.is_cached(path, path)) {
			if (::copy(ccache.cached_path(path), executable_.path())) {
				THROW("copy()", errmsg());
			}
			return;
		}

		Spawner::ExitStat es =
		   Spawner::run("cc",
		                {"cc", "-O2", path, "-o", executable_.path(), "-static",
		                 std::forward<Flags>(cc_flags)...},
		                {-1, STDOUT_FILENO, STDERR_FILENO});
		// compilation must be successful
		throw_assert(es.si.code == CLD_EXITED and es.si.status == 0);
		ccache.cache_file(path, executable_.path());
	}

	using ExitStatSiCode = decltype(std::declval<Sandbox::ExitStat>().si.code);
	using ExitStatMessage = decltype(std::declval<Sandbox::ExitStat>().message);

	static bool killed_or_dumped_by_abort(const ExitStatSiCode& si_code,
	                                      const ExitStatMessage& message) {
		return ((si_code == CLD_KILLED and
		         message == "killed by signal 6 - Aborted") or
		        (si_code == CLD_DUMPED and
		         message == "killed and dumped by signal 6 - Aborted"));
	}

	static bool killed_or_dumped_by_segv(const ExitStatSiCode& si_code,
	                                     const ExitStatMessage& message) {
		return (
		   (si_code == CLD_KILLED and
		    message == "killed by signal 11 - Segmentation fault") or
		   (si_code == CLD_DUMPED and
		    message == "killed and dumped by signal 11 - Segmentation fault"));
	}

public:
	void test_1() {
		compile_test_case("1.c");
		auto es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_EQ(es.si.code, CLD_KILLED);
		EXPECT_EQ(es.si.status, SIGKILL);
		EXPECT_EQ(es.message, "Memory limit exceeded");
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(0, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);
	}

	void test_2() {
		compile_test_case("2.c");
		auto es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_EQ(es.si.code, CLD_KILLED);
		EXPECT_EQ(es.si.status, SIGSEGV);
		EXPECT_EQ(es.message, "killed by signal 11 - Segmentation fault");
		EXPECT_EQ(es.cpu_runtime, 0s);
		EXPECT_EQ(es.runtime, 0s);
		EXPECT_EQ(es.vm_peak, 0);
	}

	void test_3() {
		compile_test_case("3.c");
		auto es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_EQ(es.si.code, CLD_EXITED);
		EXPECT_EQ(es.si.status, 37);
		EXPECT_EQ(es.message, "exited with 37");
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(0, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);
	}

	void test_4() {
		compile_test_case("4.c");
		auto es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_EQ(es.si.code, CLD_KILLED);
		EXPECT_EQ(es.si.status, SIGKILL);
		EXPECT_EQ(es.message,
		          concat_tostr("forbidden syscall: ", SYS_socket, " - socket"));
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(0, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);
	}

	void test_5() {
		compile_test_case("5.c");
		auto es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_PRED2(killed_or_dumped_by_abort, es.si.code, es.message);
		EXPECT_EQ(es.si.status, SIGABRT);
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(0, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);
	}

	void test_6() {
		compile_test_case("6.c");
		auto es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_PRED2(killed_or_dumped_by_abort, es.si.code, es.message);
		EXPECT_EQ(es.si.status, SIGABRT);
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(0, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);

		// compile_test_case("6.c"); // not needed
		es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS,
		                   {{"/tmp", OpenAccess::RDONLY}});
		EXPECT_EQ(es.si.code, CLD_EXITED);
		EXPECT_EQ(es.si.status, 0);
		EXPECT_EQ(es.message, "");
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(0, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);

		for (auto perm :
		     {OpenAccess::NONE, OpenAccess::WRONLY, OpenAccess::RDWR}) {
			// compile_test_case("6.c"); // not needed

			es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS,
			                   {{"/tmp", perm}});
			EXPECT_PRED2(killed_or_dumped_by_abort, es.si.code, es.message);
			EXPECT_EQ(es.si.status, SIGABRT);
			EXPECT_LT(0s, es.cpu_runtime);
			EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
			EXPECT_LT(0s, es.runtime);
			EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
			EXPECT_LT(0, es.vm_peak);
			EXPECT_LT(es.vm_peak, MEM_LIMIT);
		}
	}

	void test_7() {
		// Testing the allowing of lseek(), dup(), etc. on the closed stdin,
		// stdout and stderr
		compile_test_case("7.c");
		auto es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS,
		                        {{"/dev/null", OpenAccess::RDONLY}});
		EXPECT_EQ(es.si.code, CLD_EXITED);
		EXPECT_EQ(es.si.status, 0);
		EXPECT_EQ(es.message, "");
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(0, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);
	}

	void test_8() {
		// Testing the use of readlink() as the marking of the end of
		// initialization of glibc
		compile_test_case("8.c", "-m32");
		auto es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_EQ(es.si.code, CLD_KILLED);
		EXPECT_EQ(es.si.status, SIGKILL);
		EXPECT_EQ(es.message, concat_tostr("forbidden syscall: uname"));
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(0, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);

		compile_test_case("8.c", "-m64");
		es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_EQ(es.si.code, CLD_KILLED);
		EXPECT_EQ(es.si.status, SIGKILL);
		EXPECT_EQ(es.message, concat_tostr("forbidden syscall: uname"));
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(0, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);
	}

	void test_9() {
		compile_test_case("9.c", "-m32");
		auto es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_EQ(es.si.code, CLD_KILLED);
		EXPECT_EQ(es.si.status, SIGKILL);
		EXPECT_EQ(es.message,
		          concat_tostr("forbidden syscall: set_thread_area"));
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(0, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);

		compile_test_case("9.c", "-m64");
		es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_EQ(es.si.code, CLD_KILLED);
		EXPECT_EQ(es.si.status, SIGKILL);
		EXPECT_EQ(es.message,
		          concat_tostr("forbidden syscall: set_thread_area"));
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(0, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);
	}

	void test_10() {

		if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)) {
			compile_test_case("10.c", "-m32");

			auto es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
			EXPECT_EQ(es.si.code, CLD_KILLED);
			EXPECT_EQ(es.si.status, SIGKILL);
			EXPECT_EQ(es.message,
			          concat_tostr("forbidden syscall: arch_prctl"));
			EXPECT_LT(0s, es.cpu_runtime);
			EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
			EXPECT_LT(0s, es.runtime);
			EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
			EXPECT_LT(0, es.vm_peak);
			EXPECT_LT(es.vm_peak, MEM_LIMIT);
		}

		compile_test_case("10.c", "-m64");
		auto es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_EQ(es.si.code, CLD_KILLED);
		EXPECT_EQ(es.si.status, SIGKILL);
		EXPECT_EQ(es.message, concat_tostr("forbidden syscall: arch_prctl"));
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(0, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);
	}

	void test_11() {
		// Testing execve
		compile_test_case("11.c", "-m32");
		auto es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_EQ(es.si.code, CLD_KILLED);
		EXPECT_EQ(es.si.status, SIGKILL);
		EXPECT_EQ(es.message, concat_tostr("forbidden syscall: execve"));
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(0, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);

		compile_test_case("11.c", "-m64");
		es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_EQ(es.si.code, CLD_KILLED);
		EXPECT_EQ(es.si.status, SIGKILL);
		EXPECT_EQ(es.message, concat_tostr("forbidden syscall: execve"));
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(0, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);
	}

	void test_12() {
		// Tests time-outing on time and memory vm_peak calculation on timeout
		compile_test_case("12.c");
		auto es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_EQ(es.si.code, CLD_KILLED);
		EXPECT_EQ(es.si.status, SIGKILL);
		EXPECT_EQ(es.message, concat_tostr("killed by signal 9 - Killed"));
		EXPECT_LT(0s, es.cpu_runtime);
		constexpr std::chrono::nanoseconds CPU_TL_THRESHOLD =
		   1s - CPU_TIME_LIMIT;
		static_assert(
		   CPU_TIME_LIMIT + CPU_TIME_LIMIT < CPU_TL_THRESHOLD,
		   "Needed below to accurately check if the timeout occurred "
		   "early enough");
		EXPECT_LE(es.cpu_runtime, CPU_TIME_LIMIT + CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LE(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(10 << 20, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);
	}

	void test_13() {
		// Testing memory vm_peak calculation on normal exit
		compile_test_case("13.c");
		auto es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_EQ(es.si.code, CLD_EXITED);
		EXPECT_EQ(es.si.status, 0);
		EXPECT_EQ(es.message, concat_tostr(""));
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LE(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(10 << 20, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);
	}

	void test_14() {
		// Testing memory vm_peak calculation on abort
		compile_test_case("14.c");
		auto es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_PRED2(killed_or_dumped_by_abort, es.si.code, es.message);
		EXPECT_EQ(es.si.status, SIGABRT);
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LE(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(10 << 20, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);
	}

	void test_15() {
		// Testing memory vm_peak calculation on forbidden syscall
		compile_test_case("15.c");
		auto es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_EQ(es.si.code, CLD_KILLED);
		EXPECT_EQ(es.si.status, SIGKILL);
		EXPECT_EQ(es.message,
		          concat_tostr("forbidden syscall: ", SYS_socket, " - socket"));
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LE(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(10 << 20, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);
	}

	void test_16() {
		// Testing memory vm_peak calculation on forbidden syscall (according to
		// its callback)
		compile_test_case("16.c");
		auto es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_EQ(es.si.code, CLD_KILLED);
		EXPECT_EQ(es.si.status, SIGKILL);
		EXPECT_EQ(es.message, concat_tostr("forbidden syscall: execve"));
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LE(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(10 << 20, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);
	}

	void test_17() {
		// Testing memory vm_peak calculation on stack overflow
		compile_test_case("17.c");
		auto es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_PRED2(killed_or_dumped_by_segv, es.si.code, es.message);
		EXPECT_EQ(es.si.status, SIGSEGV);
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LE(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(10 << 20, es.vm_peak);
		EXPECT_LE(es.vm_peak, MEM_LIMIT);
	}

	void test_18() {
		// Testing memory vm_peak calculation on exit with exit(2)
		compile_test_case("18.c");
		auto es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_EQ(es.si.code, CLD_EXITED);
		EXPECT_EQ(es.si.status, 11);
		EXPECT_EQ(es.message, concat_tostr("exited with 11"));
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LE(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(10 << 20, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);
	}

	void test_19() {
		// Testing memory vm_peak calculation on exit with exit_group(2)
		compile_test_case("19.c");
		auto es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_EQ(es.si.code, CLD_EXITED);
		EXPECT_EQ(es.si.status, 55);
		EXPECT_EQ(es.message, concat_tostr("exited with 55"));
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LE(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(10 << 20, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);
	}

	void test_20() {
		// Testing memory vm_peak calculation on "Memory limit exceeded"
		compile_test_case("20.c");
		auto es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_EQ(es.si.code, CLD_KILLED);
		EXPECT_EQ(es.si.status, SIGKILL);
		EXPECT_EQ(es.message, "Memory limit exceeded");
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(10 << 20, es.vm_peak);
		EXPECT_LT(0, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);
	}

private:
	static std::pair<FileDescriptor, Sandbox::Options>
	dev_null_as_std_in_out_err() {
		FileDescriptor dev_null("/dev/null", O_RDWR | O_CLOEXEC);
		throw_assert(dev_null != -1);
		return {std::move(dev_null),
		        Sandbox::Options(dev_null, dev_null, dev_null, REAL_TIME_LIMIT,
		                         MEM_LIMIT, CPU_TIME_LIMIT)};
	}

public:
	void test_21() {
		auto [dev_null, rw_opts] = dev_null_as_std_in_out_err();
		// Testing writing to open stdin
		compile_test_case("21.c");
		auto es = Sandbox().run(executable_.path(), {}, rw_opts);
		EXPECT_EQ(es.si.code, CLD_EXITED);
		EXPECT_EQ(es.si.status, 1);
		EXPECT_EQ(es.message, "exited with 1");
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(0, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);

		// Testing writing to closed stdin
		// compile_test_case("21.c"); // not needed
		es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_EQ(es.si.code, CLD_EXITED);
		EXPECT_EQ(es.si.status, 7);
		EXPECT_EQ(es.message, "exited with 7");
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(0, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);
	}

	void test_22() {
		auto [dev_null, rw_opts] = dev_null_as_std_in_out_err();
		// Testing reading from open stdout and stderr
		compile_test_case("22.c");
		auto es = Sandbox().run(executable_.path(), {}, rw_opts);
		EXPECT_EQ(es.si.code, CLD_EXITED);
		EXPECT_EQ(es.si.status, 6);
		EXPECT_EQ(es.message, "exited with 6");
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(0, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);

		// Testing reading from closed stdout and stderr
		// compile_test_case("22.c"); // not needed
		es = Sandbox().run(executable_.path(), {}, SANDBOX_OPTIONS);
		EXPECT_EQ(es.si.code, CLD_EXITED);
		EXPECT_EQ(es.si.status, 7);
		EXPECT_EQ(es.message, "exited with 7");
		EXPECT_LT(0s, es.cpu_runtime);
		EXPECT_LT(es.cpu_runtime, CPU_TIME_LIMIT);
		EXPECT_LT(0s, es.runtime);
		EXPECT_LT(es.runtime, REAL_TIME_LIMIT);
		EXPECT_LT(0, es.vm_peak);
		EXPECT_LT(es.vm_peak, MEM_LIMIT);
	}
};

class SandboxTestRunner
: public concurrent::JobProcessor<void (SandboxTests::*)()> {
	const string test_cases_dir_;
	std::atomic_size_t test_ran{0};

public:
	explicit SandboxTestRunner(string test_cases_dir)
	: test_cases_dir_(std::move(test_cases_dir)) {}

protected:
	void process_job(void (SandboxTests::*test_case)()) final {
		stdlog("Running test: ", ++test_ran);
		std::invoke(test_case, SandboxTests(test_cases_dir_));
	}

	void produce_jobs() final {
		add_job(&SandboxTests::test_1);
		add_job(&SandboxTests::test_2);
		add_job(&SandboxTests::test_3);
		add_job(&SandboxTests::test_4);
		add_job(&SandboxTests::test_5);
		add_job(&SandboxTests::test_6);
		add_job(&SandboxTests::test_7);
		add_job(&SandboxTests::test_8);
		add_job(&SandboxTests::test_9);
		add_job(&SandboxTests::test_10);
		add_job(&SandboxTests::test_11);
		add_job(&SandboxTests::test_12);
		add_job(&SandboxTests::test_13);
		add_job(&SandboxTests::test_14);
		add_job(&SandboxTests::test_15);
		add_job(&SandboxTests::test_16);
		add_job(&SandboxTests::test_17);
		add_job(&SandboxTests::test_18);
		add_job(&SandboxTests::test_19);
		add_job(&SandboxTests::test_20);
		add_job(&SandboxTests::test_21);
		add_job(&SandboxTests::test_22);
	}
};

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(Sandbox, run) {
	stdlog.label(false);

	for (const auto& path : {string{"."}, executable_path(getpid())}) {
		auto tests_dir_opt =
		   deepest_ancestor_dir_with_subpath(path, "test/sandbox_test_cases/");
		if (tests_dir_opt) {
			SandboxTestRunner(*tests_dir_opt).run();
			return;
		}
	}

	FAIL() << "could not find tests directory";
}
