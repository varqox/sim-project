#include "compilation_cache.h"
#include "constants.h"
#include "sip_error.h"
#include "sip_package.h"
#include "utils.h"

#include <fstream>
#include <poll.h>
#include <simlib/filesystem.h>
#include <simlib/libzip.h>
#include <simlib/process.h>
#include <sys/inotify.h>

namespace {
constexpr std::chrono::nanoseconds DEFAULT_TIME_LIMIT = std::chrono::seconds(5);
constexpr uint64_t DEFAULT_MEMORY_LIMIT = 512; // In MiB
} // anonymous namespace

std::chrono::nanoseconds SipPackage::get_default_time_limit() {
	STACK_UNWINDING_MARK;

	// Check Sipfile
	if (access("Sipfile", F_OK) == 0) {
		sipfile.load_default_time_limit();
		return sipfile.default_time_limit;
	}

	return DEFAULT_TIME_LIMIT;
}

void SipPackage::prepare_tests_files() {
	STACK_UNWINDING_MARK;

	if (tests_files.has_value())
		return;

	tests_files = TestsFiles();
}

void SipPackage::prepare_judge_worker() {
	STACK_UNWINDING_MARK;

	if (jworker.has_value())
		return;

	jworker = sim::JudgeWorker();
	jworker.value().checker_time_limit = CHECKER_TIME_LIMIT;
	jworker.value().checker_memory_limit = CHECKER_MEMORY_LIMIT;
	jworker.value().score_cut_lambda = SCORE_CUT_LAMBDA;

	jworker.value().load_package(".", full_simfile.dump());
}

void SipPackage::generate_test_in_file(const Sipfile::GenTest& test,
	CStringView in_file)
{
	STACK_UNWINDING_MARK;

	InplaceBuff<32> generator([&]() -> InplaceBuff<32> {
		if (hasPrefix(test.generator, "sh:"))
			return InplaceBuff<32>(substring(test.generator, 3));

		if (sim::filename_to_lang(test.generator) == sim::SolutionLanguage::UNKNOWN) {
			log_warning("generator source file `", test.generator, "` has an"
					" invalid extension. It will be treated as a shell command.\n"
				"  To remove this warning you have to choose one of the"
					" following options:\n"
				"    1. Change specified generator in Sipfile to a valid source"
					" file e.g. utils/gen.cpp\n"
				"    2. Rename the generator source file to have appropriate"
					" extension e.g. utils/gen.cpp\n"
				"    3. If it is a shell command or a binary file, prefix the"
					" generator with sh: in Sipfile - e.g. sh:echo");

			return test.generator;
		}

		return CompilationCache::compile(test.generator);
	}());

	stdlog("generating ", test.name, ".in...").flush_no_nl();
	auto es = Spawner::run("sh", {
		"sh",
		"-c",
		concat_tostr(generator, ' ', test.generator_args)
	}, Spawner::Options(-1, FileDescriptor(in_file, O_WRONLY | O_CREAT | O_TRUNC), STDERR_FILENO));

	// OK
	if (es.si.code == CLD_EXITED and es.si.status == 0) {
		stdlog(" \033[1;32mdone\033[m in ",
			toString(floor_to_10ms(es.cpu_runtime), false),
			" [ RT: ", toString(floor_to_10ms(es.runtime), false), " ]");

	// RTE
	} else {
		stdlog(" \033[1;31mfailed\033[m in ",
			toString(floor_to_10ms(es.cpu_runtime), false),
			" [ RT: ", toString(floor_to_10ms(es.runtime), false), " ]"
			" (", es.message, ')');

		throw SipError("failed to generate test: ", test.name);
	}
}

sim::JudgeReport::Test SipPackage::generate_test_out_file(
	const sim::Simfile::Test& test, SipJudgeLogger& logger)
{
	STACK_UNWINDING_MARK;

	stdlog("generating ", test.name, ".out...").flush_no_nl();

	auto es = jworker.value().run_solution(test.in, test.out.value(),
		test.time_limit, test.memory_limit);

	sim::JudgeReport::Test res(
		test.name,
		sim::JudgeReport::Test::OK,
		es.cpu_runtime,
		test.time_limit,
		es.vm_peak,
		test.memory_limit,
		std::string {}
	);

	// OK
	if (es.si.code == CLD_EXITED and es.si.status == 0 and
		res.runtime <= res.time_limit)
	{

	// TLE
	} else if (res.runtime >= res.time_limit or es.runtime >= res.time_limit) {
		res.status = sim::JudgeReport::Test::TLE;
		res.comment = "Time limit exceeded";

	// MLE
	} else if (es.message == "Memory limit exceeded" or
		res.memory_consumed > res.memory_limit)
	{
		res.status = sim::JudgeReport::Test::MLE;
		res.comment = "Memory limit exceeded";

	// RTE
	} else {
		res.status = sim::JudgeReport::Test::RTE;
		res.comment = "Runtime error";
		if (es.message.size())
			back_insert(res.comment, " (", es.message, ')');
	}

	stdlog("\033[G").flush_no_nl(); // Move cursor back to the beginning of the line

	logger.test(test.name, res, es);
	return res;
}

void SipPackage::reload_simfile_from_str(std::string contents) {
	try {
		simfile = sim::Simfile(std::move(contents));
	} catch (const std::exception& e) {
		throw SipError("(Simfile) ", e.what());
	}
}

void SipPackage::reload_sipfile_from_str(std::string contents) {
	try {
		sipfile = Sipfile(std::move(contents));
	} catch (const std::exception& e) {
		throw SipError("(Sipfile) ", e.what());
	}
}

SipPackage::SipPackage() {
	STACK_UNWINDING_MARK;

	if (access("Simfile", F_OK) == 0) {
		simfile_contents = getFileContents("Simfile");
		reload_simfile_from_str(simfile_contents);
	}
	if (access("Sipfile", F_OK) == 0) {
		sipfile_contents = getFileContents("Sipfile");
		reload_sipfile_from_str(sipfile_contents);
	}
}

void SipPackage::generate_test_in_files() {
	STACK_UNWINDING_MARK;

	stdlog("\033[1;36mGenerating input files...\033[m");

	if (access("Sipfile", F_OK) != 0)
		throw SipError("No Sipfile was found");

	sipfile.load_static_tests();
	sipfile.load_gen_tests();

	// Check for tests that are both static and generated
	sipfile.static_tests.for_each([&](StringView test) {
		if (sipfile.gen_tests.find(test))
			log_warning("test `", test, "` is specified as static and as"
				" generated - treating it as generated");
	});

	prepare_tests_files();

	// Ensure that static tests have their .in files
	sipfile.static_tests.for_each([&](StringView test) {
		auto it = tests_files->tests.find(test);
		if (not it or not it->second.in.has_value()) {
			throw SipError("static test `", test, "` does not have a"
				" corresponding input file");
		}
	});

	CStringView in_dir;
	if (isDirectory("tests/"))
		in_dir = "tests/";
	else
		mkdir(in_dir = "in/");

	// Generate .in files
	sipfile.gen_tests.for_each([&](const Sipfile::GenTest& test) {
		auto it = tests_files->tests.find(test.name);
		if (not it or not it->second.in.has_value()) {
			generate_test_in_file(test,
				intentionalUnsafeCStringView(concat(in_dir, test.name, ".in")));
		} else {
			generate_test_in_file(test, it->second.in.value().to_string());
		}
	});

	// Warn about files that are not specified as static or generated
	tests_files->tests.for_each([&](auto&& p) {
		if (p.second.in.has_value() and
			not sipfile.static_tests.find(p.first) and
			not sipfile.gen_tests.find(p.first))
		{
			log_warning("test `", p.first, "` (file: ", p.second.in.value(), ")"
				" is neither specified as static nor as generated");
		}
	});

	tests_files = std::nullopt; // Probably new .in files were just created
}

static auto test_out_file(StringView test_in_file) {
	STACK_UNWINDING_MARK;

	if (hasPrefix(test_in_file, "in/")) {
		return concat<32>("out",
			test_in_file.substring(2, test_in_file.size() - 2), "out");
	}

	return concat<32>(test_in_file.substring(0, test_in_file.size() - 2), "out");
}

void SipPackage::remove_tests_with_no_in_file_from_limits_in_simfile() {
	STACK_UNWINDING_MARK;

	if (access("Simfile", F_OK) != 0)
		return; // Nothing to do (no Simfile)

	prepare_tests_files();

	// Remove tests that have no corresponding input file
	auto const& limits = simfile.config_file().get_var("limits").as_array();
	std::vector<std::string> new_limits;
	for (auto const& entry : limits) {
		StringView test_name = StringView(entry).extractLeading(not_isspace);
		auto it = tests_files->tests.find(test_name);
		if (not it or not it->second.in.has_value())
			continue;

		new_limits.emplace_back(entry);
	}

	replace_variable_in_simfile("limits",
		intentionalUnsafeStringView(simfile.dump_limits_value()), false);
}

void SipPackage::generate_test_out_files() {
	STACK_UNWINDING_MARK;

	simfile.load_interactive();
	if (simfile.interactive)
		throw SipError("Generating output files is not possible for interactive"
			" problems");

	stdlog("\033[1;36mGenerating output files...\033[m");

	prepare_tests_files();
	// Create out/ dir if needed
	tests_files.value().tests.for_each([&](auto&& p) {
		if (hasPrefix(p.second.in.value_or(""), "in/")) {
			mkdir("out");
			return false;
		}

		return true;
	});

	// We need to remove invalid entries from limits as Conver will give
	// warnings about them
	remove_tests_with_no_in_file_from_limits_in_simfile();

	// Touch .out files and give warnings
	tests_files.value().tests.for_each([&](auto&& p) {
		if (not p.second.in.has_value()) {
			log_warning("orphaned test out file: ", p.second.out.value());
			return;
		}

		if (p.second.out.has_value())
			putFileContents(concat(p.second.out.value()), "");
		else
			putFileContents(test_out_file(p.second.in.value()), "");
	});
	tests_files = std::nullopt; // New .out files may have been created

	rebuild_full_simfile(true);
	if (full_simfile.solutions.empty())
		throw SipError("no main solution was found");

	compile_solution(full_simfile.solutions.front());

	// Generate .out files and construct judge reports for adjusting time limits
	sim::JudgeReport jrep1, jrep2;
	SipJudgeLogger logger;
	logger.begin(false);
	for (auto const& group : full_simfile.tgroups) {
		auto p = sim::Simfile::TestNameComparator::split(group.tests[0].name);
		if (p.gid != "0")
			logger.begin(true); // Assumption: initial tests come as first

		auto& rep = (p.gid != "0" ? jrep2 : jrep1);
		rep.groups.emplace_back();
		for (auto const& test : group.tests) {
			rep.groups.back().tests.emplace_back(
				generate_test_out_file(test, logger));
		}
	}

	logger.end();

	// Adjust time limits
	sim::Conver::reset_time_limits_using_jugde_reports(full_simfile, jrep1,
		jrep2, conver_options(false).rtl_opts);
	jworker = std::nullopt; // Time limits have changed
}

void SipPackage::judge_solution(StringView solution) {
	STACK_UNWINDING_MARK;

	compile_solution(solution);
	compile_checker();

	// For the main solution, default time limits are used
	if (solution == full_simfile.solutions.front()) {
		auto default_time_limit = get_default_time_limit();
		for (auto& tgroup : full_simfile.tgroups)
			for (auto& test : tgroup.tests)
				test.time_limit = default_time_limit;

		jworker = std::nullopt; // Time limits have changed
	}

	prepare_judge_worker();

	stdlog('{');
	CompilationCache::load_solution(jworker.value(), solution);
	CompilationCache::load_checker(jworker.value());
	SipJudgeLogger jlogger;
	auto jrep1 = jworker.value().judge(false, jlogger);
	auto jrep2 = jworker.value().judge(true, jlogger);

	// Adjust time limits according to the model solution judge times
	if (solution == full_simfile.solutions.front()) {
		sim::Conver::reset_time_limits_using_jugde_reports(full_simfile, jrep1,
			jrep2, conver_options(false).rtl_opts);
		jworker = std::nullopt; // Time limits have changed
	}

	stdlog('}');
}

void SipPackage::compile_solution(StringView solution) {
	STACK_UNWINDING_MARK;

	prepare_judge_worker();
	stdlog("\033[1;34m", solution, "\033[m:");
	if (CompilationCache::is_cached(solution))
		stdlog("Solution is already compiled.");

	CompilationCache::load_solution(jworker.value(), solution);
}

void SipPackage::compile_checker() {
	STACK_UNWINDING_MARK;

	prepare_judge_worker();
	CompilationCache::load_checker(jworker.value());
}

void SipPackage::clean() {
	STACK_UNWINDING_MARK;

	stdlog("Cleaning...").flush_no_nl();

	CompilationCache::clear();
	if (remove_r("utils/latex/") and errno != ENOENT)
		THROW("remove_r()", errmsg());

	(void)rmdir("utils/"); // Remove utils/ directory if empty

	stdlog(" done.");
}

void SipPackage::remove_generated_test_files() {
	STACK_UNWINDING_MARK;

	sipfile.load_gen_tests();
	prepare_tests_files();

	stdlog("Removing generated test files...").flush_no_nl();
	// Remove generated .in files
	sipfile.gen_tests.for_each([&](auto const& test) {
		auto it = tests_files->tests.find(test.name);
		if (it and it->second.in.has_value())
			remove(it->second.in.value().to_string());
	});

	// Remove generated .out files
	tests_files->tests.for_each([](auto const& p) {
		if (p.second.in.has_value() and p.second.out.has_value())
			remove(p.second.out.value().to_string());
	});

	stdlog(" done.");
}

void SipPackage::remove_test_files_not_specified_in_sipfile() {
	STACK_UNWINDING_MARK;

	sipfile.load_static_tests();
	sipfile.load_gen_tests();
	prepare_tests_files();

	stdlog("Removing test files that are not specified as generated or"
		" static...").flush_no_nl();

	tests_files->tests.for_each([&](auto const& p) {
		bool is_static = sipfile.static_tests.find(p.first);
		bool is_generated = sipfile.gen_tests.find(p.first);
		if (is_static or is_generated)
			return;

		if (p.second.in.has_value())
			remove(concat(p.second.in.value()));
		if (p.second.out.has_value())
			remove(concat(p.second.out.value()));
	});

	tests_files = std::nullopt; // Probably some test files were just removed
	stdlog(" done.");
}

sim::Conver::Options SipPackage::conver_options(bool set_default_time_limits) {
	STACK_UNWINDING_MARK;

	sim::Conver::Options copts;
	copts.reset_time_limits_using_model_solution = set_default_time_limits;
	copts.ignore_simfile = false;
	copts.seek_for_new_tests = true;
	copts.reset_scoring = false;
	copts.require_statement = false;
	copts.rtl_opts.min_time_limit = MIN_TIME_LIMIT;
	copts.rtl_opts.solution_runtime_coefficient = SOLUTION_RUNTIME_COEFFICIENT;

	// Check Simfile
	if (access("Simfile", F_OK) == 0) {
		try {
			simfile.load_name();
		} catch (...) {
			log_warning("no problem name was specified in Simfile");
			copts.name = "unknown";
		}
	} else {
		copts.memory_limit = DEFAULT_MEMORY_LIMIT;
		copts.name = "unknown";
	}

	using std::chrono_literals::operator""ns;

	copts.max_time_limit =
		std::chrono::duration_cast<decltype(copts.max_time_limit)>(
			sim::Conver::solution_runtime_to_time_limit(
				get_default_time_limit(), SOLUTION_RUNTIME_COEFFICIENT,
				MIN_TIME_LIMIT) + 0.5ns);

	return copts;
}

void SipPackage::rebuild_full_simfile(bool set_default_time_limits) {
	STACK_UNWINDING_MARK;

	sim::Conver conver;
	conver.package_path(".");
	sim::Conver::ConstructionResult cr =
		conver.construct_simfile(conver_options(set_default_time_limits));

	// Filter out every line except warnings
	StringView report = conver.report();
	size_t line_beg = 0, line_end = report.find('\n');
	size_t next_warning = report.find("warning");
	while (next_warning != StringView::npos) {
		if (next_warning < line_end)
			stdlog(report.substring(line_beg, line_end));

		if (line_end == StringView::npos or line_end + 1 == report.size())
			break;

		line_beg = line_end + 1;
		line_end = report.find('\n', line_beg);
		if (next_warning < line_beg)
			next_warning = report.find(line_beg, "warning");
	}

	// Ignore the need for the model solution to set the time limits - the
	// time limits will be set to max_time_limit
	// throw_assert(cr.status == sim::Conver::Status::COMPLETE);

	full_simfile = std::move(cr.simfile);
	jworker = std::nullopt; // jworker has become outdated
}

void SipPackage::save_scoring() {
	STACK_UNWINDING_MARK;
	stdlog("Saving scoring...").flush_no_nl();

	replace_variable_in_simfile("scoring",
		intentionalUnsafeStringView(full_simfile.dump_scoring_value()), false);

	stdlog(" done.");
}

void SipPackage::save_limits() {
	STACK_UNWINDING_MARK;
	stdlog("Saving limits...").flush_no_nl();

	replace_variable_in_simfile("limits",
		intentionalUnsafeStringView(full_simfile.dump_limits_value()), false);

	stdlog(" done.");
}

static void compile_tex_file(StringView file) {
	STACK_UNWINDING_MARK;

	stdlog("\033[1mCompiling ", file, "\033[m");
	// It is necessary (essential) to run latex two times
	for (int iter = 0; iter < 2; ++iter) {
		auto es = Spawner::run("pdflatex", {
			"pdflatex",
			"-output-dir=utils/latex",
			file.to_string()
		}, {-1, STDOUT_FILENO, STDERR_FILENO});

		if (es.si.code != CLD_EXITED or es.si.status != 0) {
			// During watching it is not intended to stop on compilation error
			errlog("\033[1;31mCompilation failed.\033[m");
		}
	}

	move(concat("utils/latex/", filename(file).withoutSuffix(3), "pdf"),
		concat(file.withoutSuffix(3), "pdf"));
}

static void watch_tex_files(const std::vector<std::string>& tex_files) {
	STACK_UNWINDING_MARK;

	FileDescriptor ino_fd(inotify_init());
	if (ino_fd == -1)
		THROW("inotify_init()", errmsg());

	AVLDictSet<CStringView> unwatched_files;
	for (CStringView file : tex_files)
		unwatched_files.emplace(file);

	AVLDictMap<FileDescriptor, CStringView> watched_files; // fd => file
	auto process_unwatched_files = [&] {
		for (auto it = unwatched_files.front(); it; ) {
			CStringView file = *it;
			FileDescriptor fd(inotify_add_watch(ino_fd, file.data(),
				IN_MODIFY | IN_MOVE_SELF));
			if (fd == -1) {
				log_warning("could not watch file ", file, ":"
					" inotify_add_watch()", errmsg());

				it = unwatched_files.upper_bound(file);
				continue;
			}

			// File is now watched
			stdlog("\033[1mStarted watching ", file, "\033[m");
			watched_files.emplace(std::move(fd), file);
			it = unwatched_files.upper_bound(file);
			unwatched_files.erase(file);
		}
	};

	using std::chrono::milliseconds;
	using std::chrono_literals::operator""ms;
	constexpr milliseconds STILNESS_THRESHOLD = 10ms;

	// Inotify buffer
	// WARNING: this assumes that no directory is watched
	char inotify_buf[sizeof(inotify_event) * tex_files.size()];
	for (AVLDictSet<CStringView> files_to_recompile;;) {
		process_unwatched_files();

		// Several normal events on the same file may occur in a quick
		// succession and running latex compiler while the file is still
		// changing is not a good idea. So we collect the events till a
		// stillness moment occurs and then we compile them. It is not an ideal
		// solution (waiting for stillness on every file), but works well in
		// practice and it seems there is no need to implement move
		// sophisticated one (monitoring stillness on each file individually)

		// Wait for notification
		pollfd pfd = {ino_fd, POLLIN, 0};
		int rc = poll(&pfd, 1, milliseconds(STILNESS_THRESHOLD).count());
		if (rc == 0) {
			// Stillness happened -> safe to recompile files that changed
			files_to_recompile.for_each(compile_tex_file);
			files_to_recompile.clear();
			rc = poll(&pfd, 1, -1); // Wait indefinitely for new events
		}

		if (rc < 0) // Handles error of the first or the second poll
			THROW("poll()", errmsg());

		ssize_t len = read(ino_fd, inotify_buf, sizeof(inotify_buf));
		if (len < 1) {
			log_warning("read()", errmsg());
			continue;
		}

		struct inotify_event *event;
		// Process files for which an event occurred
		for (char *ptr = inotify_buf; ptr < inotify_buf + len;
			ptr += sizeof(inotify_event)) // WARNING: if you want to watch
			                              // directories, add + events->len
		{
			event = (struct inotify_event*)ptr;
			CStringView file = watched_files.find(event->wd)->second;
			auto unwatch_file = [&] {
				unwatched_files.emplace(file);
				watched_files.erase(event->wd);
			};

			// If file was moved, stop watching it
			if (event->mask & IN_MOVE_SELF) {
				if (inotify_rm_watch(ino_fd, event->wd))
					THROW("inotify_rm_watch()", errmsg());
				unwatch_file();

			// If file has disappeared, stop watching it
			} else if (event->mask & IN_IGNORED) {
				unwatch_file();

			// Other (normal) event occurred
			} else {
				files_to_recompile.emplace(file);
			}
		}
	}
}

void SipPackage::compile_tex_files(bool watch) {
	STACK_UNWINDING_MARK;

	sim::PackageContents pc;
	pc.load_from_directory(".");

	std::vector<std::string> tex_files;
	pc.for_each_with_prefix("", [&](StringView file) {
		if (hasSuffix(file, ".tex"))
			tex_files.emplace_back(file.to_string());
	});

	if (tex_files.empty()) {
		log_warning("no .tex file was found");
		return;
	}

	mkdir_r("utils/latex");
	for (auto const& file : tex_files)
		compile_tex_file(file);

	if (watch)
		watch_tex_files(tex_files);
}

void progress_callback(zip_t*, double p, void*) {
	using namespace std::chrono;
	stdlog("\033[2K\033[GZipping... Progress: ",
		toString(duration<int, std::deci>(int(p * 1e3)), false),
		'%').flush_no_nl();
}

void SipPackage::archive_into_zip(CStringView dest_file) {
	STACK_UNWINDING_MARK;

	stdlog("Zipping...").flush_no_nl();

	// Create zip
	{
		ZipFile zip(concat("../", dest_file, ".zip"), ZIP_CREATE | ZIP_TRUNCATE);
		zip_register_progress_callback_with_state(zip, 0.001, progress_callback, nullptr, nullptr);
		sim::PackageContents pkg;
		pkg.load_from_directory(".");
		pkg.for_each_with_prefix("", [&](StringView path) {
			auto name = concat(dest_file, '/', path);
			if (name.back() == '/')
				zip.dir_add(name);
			else
				zip.file_add(name, zip.source_file(concat(path)));
		});

		zip.close();
	}

	stdlog("\033[2K\033[GZipping... done.");
}

void SipPackage::replace_variable_in_configfile(const ConfigFile& cf,
	FilePath configfile_path, StringView configfile_contents,
	StringView var_name, StringView replacement, bool escape_replacement)
{
	STACK_UNWINDING_MARK;

	std::ofstream out(configfile_path.data());
	auto var = cf.get_var(var_name);
	if (var.is_set()) {
		auto var_span = var.value_span;
		out.write(configfile_contents.data(), var_span.beg);
		if (not replacement.empty()) {
			if (escape_replacement)
				out << ConfigFile::escape_string(replacement);
			else
				out.write(replacement.data(), replacement.size());
		}
		out.write(configfile_contents.data() + var_span.end,
			configfile_contents.size() - var_span.end);

	} else {
		out.write(configfile_contents.data(), configfile_contents.size());
		if (not configfile_contents.empty() and configfile_contents.back() != '\n')
			out << '\n';

		if (replacement.empty()) {
			out << var_name << ": \n";
		} else {
			out << var_name << ": ";
			if (escape_replacement)
				out << ConfigFile::escape_string(replacement);
			else
				out.write(replacement.data(), replacement.size());
			out << '\n';
		}
	}
}

void SipPackage::replace_variable_in_configfile(const ConfigFile& cf,
	FilePath configfile_path, StringView configfile_contents,
	StringView var_name, const std::vector<std::string>& replacement)
{
	STACK_UNWINDING_MARK;

	std::ofstream out(configfile_path.data());
	auto print_replacement = [&] {
		out << "[";
		if (not replacement.empty()) {
			out << "\n\t" << ConfigFile::escape_string(replacement[0]);
			for (uint i = 1; i < replacement.size(); ++i)
				out << ",\n\t" << ConfigFile::escape_string(replacement[i]);
			out << '\n';
		}
		out << ']';
	};

	auto var = cf.get_var(var_name);
	if (var.is_set()) {
		auto var_span = var.value_span;
		out.write(configfile_contents.data(), var_span.beg);
		print_replacement();
		out.write(configfile_contents.data() + var_span.end,
			configfile_contents.size() - var_span.end);

	} else {
		out.write(configfile_contents.data(), configfile_contents.size());
		if (configfile_contents.back() != '\n')
			out << '\n';

		out << var_name << ": ";
		print_replacement();
		out << '\n';
	}
}

void SipPackage::replace_variable_in_simfile(StringView var_name,
	StringView replacement, bool escape_replacement)
{
	STACK_UNWINDING_MARK;

	replace_variable_in_configfile(simfile.config_file(), "Simfile",
		simfile_contents, var_name, replacement, escape_replacement);
	simfile_contents = getFileContents("Simfile");
	reload_simfile_from_str(simfile_contents);
}

void SipPackage::replace_variable_in_simfile(StringView var_name,
	const std::vector<std::string>& replacement)
{
	STACK_UNWINDING_MARK;

	replace_variable_in_configfile(simfile.config_file(), "Simfile",
		simfile_contents, var_name, replacement);
	simfile_contents = getFileContents("Simfile");
	reload_simfile_from_str(simfile_contents);
}

void SipPackage::replace_variable_in_sipfile(StringView var_name,
	StringView replacement, bool escape_replacement)
{
	STACK_UNWINDING_MARK;

	replace_variable_in_configfile(sipfile.config_file(), "Sipfile",
		sipfile_contents, var_name, replacement, escape_replacement);
	sipfile_contents = getFileContents("Sipfile");
	reload_sipfile_from_str(sipfile_contents);
}

void SipPackage::replace_variable_in_sipfile(StringView var_name,
	const std::vector<std::string>& replacement)
{
	STACK_UNWINDING_MARK;

	replace_variable_in_configfile(sipfile.config_file(), "Sipfile",
		sipfile_contents, var_name, replacement);
	sipfile_contents = getFileContents("Sipfile");
	reload_sipfile_from_str(sipfile_contents);
}

void SipPackage::create_default_directory_structure() {
	STACK_UNWINDING_MARK;

	stdlog("Creating directories...").flush_no_nl();

	if ((mkdir("check") == -1 and errno != EEXIST)
		or (mkdir("doc") == -1 and errno != EEXIST)
		or (mkdir("in") == -1 and errno != EEXIST)
		or (mkdir("prog") == -1 and errno != EEXIST)
		or (mkdir("utils") == -1 and errno != EEXIST))
	{
		THROW("mkdir()", errmsg());
	}

	stdlog(" done.");
}

void SipPackage::create_default_sipfile() {
	STACK_UNWINDING_MARK;

	if (access("Sipfile", F_OK) == 0) {
		stdlog("Sipfile already created");
		return;
	}

	stdlog("Creating Sipfile...").flush_no_nl();

	const auto default_sipfile_contents = concat_tostr(
		"default_time_limit: ", toString(DEFAULT_TIME_LIMIT), "\n"
		"static: [\n"
			"\t# Here provide tests that are \"hard-coded\"\n"
			"\t# Syntax: <test-range>\n"
		"]\ngen: [\n"
			"\t# Here provide rules to generate tests\n"
			"\t# Syntax: <test-range> <generator> [generator arguments]\n"
		"]\n");

	reload_sipfile_from_str(default_sipfile_contents);
	putFileContents("Sipfile", default_sipfile_contents);

	stdlog(" done.");
}

void SipPackage::create_default_simfile(Optional<CStringView> problem_name) {
	STACK_UNWINDING_MARK;

	stdlog("Creating Simfile...");

	auto package_dir_filename = filename(intentionalUnsafeStringView(getCWD()).withoutSuffix(1)).to_string();
	if (access("Simfile", F_OK) == 0) {
		simfile_contents = getFileContents("Simfile");
		reload_simfile_from_str(simfile_contents);

		// Problem name
		auto replace_name_in_simfile = [&](StringView replacement) {
			stdlog("Overwriting the problem name with: ", replacement);
			replace_variable_in_simfile("name", replacement);
		};

		if (problem_name.has_value()) {
			replace_name_in_simfile(problem_name.value());
		} else {
			try { simfile.load_name(); } catch (const std::exception& e) {
				stdlog("\033[1;35mwarning\033[m: ignoring: ", e.what());

				if (package_dir_filename.empty()) {
					throw SipError("problem name was neither found in Simfile"
						", nor provided as a command-line argument"
						" nor deduced from the provided package path");
				}

				replace_name_in_simfile(package_dir_filename);
			}
		}

		// Memory limit
		if (not simfile.config_file().get_var("memory_limit").is_set())
			replace_variable_in_simfile("memory_limet",
				intentionalUnsafeStringView(toStr(DEFAULT_MEMORY_LIMIT)));

	} else if (problem_name.has_value()) {
		stdlog("name = ", problem_name.value());
		putFileContents("Simfile", intentionalUnsafeStringView(concat(
			"name: ", ConfigFile::escape_string(problem_name.value()),
			"\nmemory_limit: ", DEFAULT_MEMORY_LIMIT, '\n')));

	} else if (not package_dir_filename.empty()) {
		stdlog("name = ", package_dir_filename);
		putFileContents("Simfile", intentionalUnsafeStringView(concat(
			"name: ", ConfigFile::escape_string(package_dir_filename),
			"\nmemory_limit: ", DEFAULT_MEMORY_LIMIT, '\n')));

	} else { // iff absolute path of dir is /
		throw SipError("problem name was neither provided as a command-line"
			" argument nor deduced from the provided package path");
	}
}
