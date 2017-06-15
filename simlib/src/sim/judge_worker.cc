#include "../../include/parsers.h"
#include "../../include/sim/checker.h"
#include "../../include/sim/judge_worker.h"

using std::string;

namespace {

class JudgeCallback : protected Sandbox::DefaultCallback {
public:
	JudgeCallback() = default;

	using DefaultCallback::detectTraceeArchitecture;
	using DefaultCallback::getArch;
	using DefaultCallback::isSyscallExitAllowed;
	using DefaultCallback::errorMessage;

	bool isSyscallEntryAllowed(pid_t pid, int syscall) {
		constexpr int sys_nanosleep[2] = {
			162, // SYS_nanosleep - i386
			35, // SYS_nanosleep - x86_64
		};
		constexpr int sys_clock_nanosleep[2] = {
			267, // SYS_clock_nanosleep - i386
			230, // SYS_clock_nanosleep - x86_64
		};

		if (syscall != sys_nanosleep[arch] and
			syscall != sys_clock_nanosleep[arch])
		{
			return DefaultCallback::isSyscallExitAllowed(pid, syscall);
		}

		Registers regs;
		regs.getRegs(pid);

		if (syscall == sys_nanosleep[arch]) {
			// Set NULL as the first argument to nanosleep(2)
			if (arch)
				regs.uregs.x86_64_regs.rdi = 0;
			else
				regs.uregs.i386_regs.ebx = 0;

		} else {
			// Set NULL as the third argument to clock_nanosleep(2)
			if (arch)
				regs.uregs.x86_64_regs.rdx = 0;
			else
				regs.uregs.i386_regs.edx = 0;
		}

		regs.setRegs(pid); // Update traced process's registers
		return true;
	}
};

} // anonymous namespace

namespace sim {

constexpr meta::string JudgeWorker::CHECKER_FILENAME;
constexpr meta::string JudgeWorker::SOLUTION_FILENAME;
constexpr timespec JudgeWorker::CHECKER_TIME_LIMIT;


JudgeReport JudgeWorker::judge(bool final) const {
	auto vlog = [&](auto&&... args) {
		if (verbose)
			stdlog(std::forward<decltype(args)>(args)...);
	};

	vlog("Judging on `", pkg_root,"` (", (final ? "final" : "initial"), "): {");

	string sol_stdout_path {tmp_dir.path() + "sol_stdout"};
	// Checker STDOUT
	FileDescriptor checker_stdout {openUnlinkedTmpFile()};
	if (checker_stdout < 0)
		THROW("Failed to create unlinked temporary file", error(errno));

	// Solution STDOUT
	FileDescriptor solution_stdout(sol_stdout_path,
		O_WRONLY | O_CREAT | O_TRUNC);
	if (solution_stdout < 0)
		THROW("Failed to open file `", sol_stdout_path, '`', error(errno));

	FileRemover solution_stdout_remover {sol_stdout_path}; // Save disk space

	const int page_size = sysconf(_SC_PAGESIZE);

	// Checker parameters
	Sandbox::Options checker_opts = {
		-1, // STDIN is ignored
		checker_stdout, // STDOUT
		-1, // STDERR is ignored
		CHECKER_TIME_LIMIT,
		CHECKER_MEMORY_LIMIT + page_size // To be able to detect exceeding
	};

	JudgeReport report;
	string checker_path {concat_tostr(tmp_dir.path(), CHECKER_FILENAME)};
	string solution_path {concat_tostr(tmp_dir.path(), SOLUTION_FILENAME)};

	for (auto&& group : sf.tgroups) {
		// Group "0" goes to the initial report, others groups to final
		auto p = Simfile::TestNameComparator::split(group.tests[0].name);
		if ((p.first != "0") != final)
			continue;

		report.groups.emplace_back();
		auto& report_group = report.groups.back();

		double score_ratio = 1.0;

		for (auto&& test : group.tests) {
			// Prepare solution fds
			(void)ftruncate(solution_stdout, 0);
			(void)lseek(solution_stdout, 0, SEEK_SET);

			string test_in_path {pkg_root + test.in};
			string test_out_path {pkg_root + test.out};

			FileDescriptor test_in(test_in_path, O_RDONLY | O_LARGEFILE);
			if (test_in < 0)
				THROW("Failed to open file `", test_in_path, '`', error(errno));

			// Set time limit to 1.5 * time_limit + 1 s, because CPU time is
			// measured
			uint64_t real_tl = test.time_limit * 3 / 2 + 1000000;
			timespec tl;
			tl.tv_sec = real_tl / 1000000;
			tl.tv_nsec = (real_tl - tl.tv_sec * 1000000) * 1000;

			// Run solution on the test
			Sandbox::ExitStat es = Sandbox::run(solution_path, {}, {
				test_in,
				solution_stdout,
				-1,
				tl,
				test.memory_limit + page_size, // To be able to detect exceeding
				// CPU time limit - give at leas 0.3 s margin to mitigate the
				// inaccuracy of measurements
				(test.time_limit + 1300000) / 1000000,
			}, ".", JudgeCallback{}); // Allow exceptions to fly upper

			// Log problems with syscalls
			if (verbose && hasPrefixIn(es.message,
				{"Error: ", "failed to get syscall", "forbidden syscall"}))
			{
				errlog("Package: `", pkg_root, "` ", test.name, " -> ",
					es.message);
			}

			// Update score_ratio
			timeval cpu_runtime = es.rusage.ru_utime + es.rusage.ru_stime;
			score_ratio = std::min(score_ratio, 2.0 -
				2.0 * (cpu_runtime.tv_sec * 100 + cpu_runtime.tv_usec / 10000)
				/ (test.time_limit / 10000));
			// cpu_runtime may be grater than test.time_limit therefore
			// score_ratio may be negative which is impermissible
			if (score_ratio < 0)
				score_ratio = 0;

			report_group.tests.emplace_back(
				test.name,
				JudgeReport::Test::OK,
				cpu_runtime.tv_sec * 1000000 + cpu_runtime.tv_usec,
				test.time_limit,
				es.vm_peak,
				test.memory_limit,
				std::string {}
			);
			auto& test_report = report_group.tests.back();

			auto do_logging = [&] {
				if (!verbose)
					return;

				auto tmplog = stdlog("  ", paddedString(test.name, 11, LEFT),
					paddedString(
						usecToSecStr(test_report.runtime, 2, false), 4),
					" / ", usecToSecStr(test_report.time_limit, 2, false),
					" s  ", test_report.memory_consumed >> 10, " / ",
					test_report.memory_limit >> 10, " KB"
					"    Status: ");
				// Status
				switch (test_report.status) {
				case JudgeReport::Test::TLE: tmplog("\033[1;33mTLE\033[m");
					break;
				case JudgeReport::Test::MLE: tmplog("\033[1;33mMLE\033[m");
					break;
				case JudgeReport::Test::RTE: tmplog("\033[1;31mRTE\033[m (",
					es.message, ')'); break;
				default:
					THROW("Should not reach here");
				}
				// Rest
				if (es.si.code == CLD_EXITED)
					tmplog("   Exited with ", es.si.status);
				else
					tmplog("   Killed with ", es.si.status, " (",
						strsignal(es.si.status), ')');

				tmplog(" [ CPU: ", timeval_to_str(cpu_runtime, 6, false), " ]"
					" [ ", timespec_to_str(es.runtime, 9, false), " ]");
			};

			// OK
			if (es.si.code == CLD_EXITED and es.si.status == 0 and
				test_report.runtime <= test_report.time_limit)
			{

			// TLE
			} else if (test_report.runtime >= test_report.time_limit or
				es.runtime == tl)
			{
				// If works if es.runtime == tl which means that real_time_limit
				// exceeded
				if (test_report.runtime < test_report.time_limit)
					test_report.runtime = test_report.time_limit;

				// After checking status for OK `>=` comparison is safe to
				// detect exceeding
				score_ratio = 0;
				test_report.status = JudgeReport::Test::TLE;
				test_report.comment = "Time limit exceeded";
				do_logging();
				continue;

			// MLE
			} else if (es.message == "Memory limit exceeded" ||
				test_report.memory_consumed > test_report.memory_limit)
			{
				score_ratio = 0;
				test_report.status = JudgeReport::Test::MLE;
				test_report.comment = "Memory limit exceeded";
				do_logging();
				continue;

			// RTE
			} else {
				score_ratio = 0;
				test_report.status = JudgeReport::Test::RTE;
				test_report.comment = "Runtime error";
				if (es.message.size())
					back_insert(test_report.comment, " (", es.message, ')');

				do_logging();
				continue;
			}

			// Status == OK

			/* Checking solution output with checker */

			// Prepare checker fds
			(void)ftruncate(checker_stdout, 0);
			(void)lseek(checker_stdout, 0, SEEK_SET);

			// Run checker
			auto ces = Sandbox::run(checker_path,
				{checker_path, test_in_path, test_out_path, sol_stdout_path},
				checker_opts,
				".",
				CheckerCallback({test_in_path, test_out_path, sol_stdout_path})
			); // Allow exceptions to fly upper

			auto append_checker_rt_info = [&] {
				back_insert(test_report.comment, "; ",
					timespec_to_str(ces.runtime, 2), " / ",
					timespec_to_str(checker_opts.real_time_limit, 2), " s  ",
					ces.vm_peak >> 10, " /  ", checker_opts.memory_limit >> 10,
					" KB");
			};

			// Checker exited with 0
			if (ces.si.code == CLD_EXITED and ces.si.status == 0) {
				string chout = sim::obtainCheckerOutput(checker_stdout, 512);
				SimpleParser s {chout};

				StringView line1 {s.extractNext('\n')}; // "OK" or "WRONG"
				StringView line2 {s.extractNext('\n')}; // percentage (real)
				// Second line has to be either empty or be a real number
				if (line2.size() && (line2[0] == '-' || !isReal(line2))) {
					score_ratio = 0; // Do not give score for a checker error
					test_report.status = JudgeReport::Test::CHECKER_ERROR;
					test_report.comment = concat_tostr("Checker error: second "
						"line of the checker's stdout is invalid: `", line2,
						'`');
					append_checker_rt_info();

				// OK -> Checker: OK
				} else if (line1 == "OK") {
					// Test status is already set to OK
					if (line2.size()) // Empty line means 100%
						score_ratio = std::min(score_ratio,
							atof(line2.to_string().data()) * 0.01); /*
								.to_string() is safer than tricky indexing over
								chout */

					// Leave the checker comment only
					chout.erase(chout.begin(), chout.end() - s.size());
					test_report.comment = std::move(chout);

				// WRONG -> Checker WA
				} else {
					test_report.status = JudgeReport::Test::WA;
					score_ratio = 0;
					// Leave the checker comment only
					chout.erase(chout.begin(), chout.end() - s.size());
					test_report.comment = std::move(chout);
				}

			// Checker TLE
			} else if (ces.runtime >= CHECKER_TIME_LIMIT) {
				score_ratio = 0; // Do not give score for a checker error
				test_report.status = JudgeReport::Test::CHECKER_ERROR;
				test_report.comment = "Checker: time limit exceeded";
				append_checker_rt_info();

			// Checker MLE
			} else if (ces.message == "Memory limit exceeded" ||
				ces.vm_peak > CHECKER_MEMORY_LIMIT)
			{
				score_ratio = 0; // Do not give score for a checker error
				test_report.status = JudgeReport::Test::CHECKER_ERROR;
				test_report.comment = "Checker: memory limit exceeded";
				append_checker_rt_info();

			// Checker RTE
			} else {
				score_ratio = 0; // Do not give score for a checker error
				test_report.status = JudgeReport::Test::CHECKER_ERROR;
				test_report.comment = "Checker runtime error";
				if (ces.message.size())
					back_insert(test_report.comment, " (", ces.message, ')');
				append_checker_rt_info();
			}

			if (verbose) {
				auto tmplog = stdlog("  ", paddedString(test.name, 11, LEFT),
					paddedString(
						usecToSecStr(test_report.runtime, 2, false), 4),
					" / ", usecToSecStr(test_report.time_limit, 2, false),
					" s  ", test_report.memory_consumed >> 10, " / ",
					test_report.memory_limit >> 10, " KB"
					"    Status: \033[1;32mOK\033[m");
				// Rest
				if (es.si.code == CLD_EXITED)
					tmplog("   Exited with ", es.si.status);
				else
					tmplog("   Killed with ", es.si.status, " (",
						strsignal(es.si.status), ')');

				tmplog(" [ CPU: ", timeval_to_str(cpu_runtime, 6, false), " ]"
					" [ ", timespec_to_str(es.runtime, 9, false), " ]"
					"  Checker: ");

				// Checker status
				if (test_report.status == JudgeReport::Test::OK)
					tmplog("\033[1;32mOK\033[m  ", test_report.comment);
				else if (test_report.status == JudgeReport::Test::WA)
					tmplog("\033[1;31mWRONG\033[m ", test_report.comment);
				else
					tmplog("\033[1;33mERROR\033[m ", test_report.comment);
				// Rest
				if (ces.si.code == CLD_EXITED)
					tmplog("   Exited with ", ces.si.status);
				else
					tmplog("   Killed with ", ces.si.status, " (",
						strsignal(ces.si.status), ')');

				tmplog(" [ ", timespec_to_str(ces.runtime, 9, false), " ]  ",
					ces.vm_peak >> 10, " / ", CHECKER_MEMORY_LIMIT >> 10,
					" KB");
			}
		}

		// Compute group score
		report_group.score = round(group.score * score_ratio);
		report_group.max_score = group.score;

		vlog("  Score: ", report_group.score, " / ", report_group.max_score,
			" (ratio: ", toStr(score_ratio, 3), ')');
	}

	vlog('}');
	return report;
}

} // namespace sim
