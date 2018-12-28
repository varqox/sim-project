#include "compilation_cache.h"
#include "sip_error.h"
#include "sip_package.h"

#include <fstream>
#include <simlib/filesystem.h>
#include <simlib/process.h>

namespace {
constexpr uint64_t DEFAULT_TIME_LIMIT = 5; // In seconds
constexpr uint64_t DEFAULT_MEMORY_LIMIT = 512; // In MB
} // anonymous namespace

uint64_t SipPackage::get_default_time_limit() {
	// Check Sipfile
	if (access("Sipfile", F_OK) == 0) {
		sipfile.loadDefaultTimeLimit();
		return sipfile.default_time_limit;
	}

	return DEFAULT_TIME_LIMIT * 1'000'000;
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
	jworker.value().loadPackage(".", full_simfile.dump());
}

static inline uint64_t timespec_to_usec(timespec t) noexcept {
	return t.tv_sec * 1000000 + t.tv_nsec / 1000;
}

void SipPackage::generate_test_in_file(const Sipfile::GenTest& test,
	CStringView in_file)
{
	STACK_UNWINDING_MARK;

	InplaceBuff<32> generator([&]() -> InplaceBuff<32> {
		if (hasPrefix(test.generator, "sh:"))
			return InplaceBuff<32>(substring(test.generator, 3));

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
			timespec_to_str(es.cpu_runtime, 2, false),
			" [ RT: ", timespec_to_str(es.runtime, 2, false), " ]");

	// RTE
	} else {
		stdlog(" \033[1;31mfailed\033[m in ",
			timespec_to_str(es.cpu_runtime, 2, false),
			" [ RT: ", timespec_to_str(es.runtime, 2, false), " ]"
			" (", es.message, ')');

		throw SipError("failed to generate test: ", test.name);
	}
}

sim::JudgeReport::Test SipPackage::generate_test_out_file(
	const sim::Simfile::Test& test, SipJudgeLogger& logger)
{
	STACK_UNWINDING_MARK;

	stdlog("generating ", test.name, ".out...").flush_no_nl();

	auto es = jworker.value().run_solution(test.in, test.out, test.time_limit, test.memory_limit);

	sim::JudgeReport::Test res(
		test.name,
		sim::JudgeReport::Test::OK,
		timespec_to_usec(es.cpu_runtime),
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
	} else if (res.runtime >= res.time_limit or
		timespec_to_usec(es.runtime) == res.time_limit)
	{
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

SipPackage::SipPackage() {
	if (access("Simfile", F_OK) == 0) {
		simfile_contents = getFileContents("Simfile");
		simfile = sim::Simfile(simfile_contents);
	}
	if (access("Sipfile", F_OK) == 0) {
		sipfile_contents = getFileContents("Sipfile");
		sipfile = Sipfile(sipfile_contents);
	}
}

void SipPackage::generate_test_in_files() {
	STACK_UNWINDING_MARK;

	stdlog("\033[1;36mGenerating input files...\033[m");

	sipfile.loadStaticTests();
	sipfile.loadGenTests();

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
	if (hasPrefix(test_in_file, "in/")) {
		return concat<32>("out",
			test_in_file.substring(2, test_in_file.size() - 2), "out");
	}

	return concat<32>(test_in_file.substring(0, test_in_file.size() - 2), "out");
}

void SipPackage::generate_test_out_files() {
	STACK_UNWINDING_MARK;

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
	logger.begin({}, false);
	for (auto const& group : full_simfile.tgroups) {
		auto p = sim::Simfile::TestNameComparator::split(group.tests[0].name);
		if (p.gid != "0")
			logger.begin({}, true); // Assumption: initial tests come as first

		auto& rep = (p.gid != "0" ? jrep2 : jrep1);
		rep.groups.emplace_back();
		for (auto const& test : group.tests) {
			rep.groups.back().tests.emplace_back(
				generate_test_out_file(test, logger));
		}
	}

	logger.end();

	// Adjust time limits
	sim::Conver::finishConstructingSimfile(full_simfile, jrep1, jrep2);
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
		sim::Conver::finishConstructingSimfile(full_simfile, jrep1, jrep2);
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

	stdlog(" done.");
}

void SipPackage::remove_generated_test_files() {
	STACK_UNWINDING_MARK;

	sipfile.loadGenTests();
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

sim::Conver::Options SipPackage::conver_options(bool set_default_time_limits) {
	STACK_UNWINDING_MARK;

	sim::Conver::Options copts;
	copts.reset_time_limits_using_model_solution = set_default_time_limits;
	copts.ignore_simfile = false;
	copts.seek_for_new_tests = true;
	copts.reset_scoring = false;
	copts.require_statement = false;

	// Check Simfile
	if (access("Simfile", F_OK) == 0) {
		try {
			simfile.loadName();
		} catch (...) {
			log_warning("no problem name was specified in Simfile");
			copts.name = "unknown";
		}
	} else {
		copts.memory_limit = DEFAULT_MEMORY_LIMIT;
		copts.name = "unknown";
	}

	copts.max_time_limit = get_default_time_limit();
	return copts;
}

void SipPackage::rebuild_full_simfile(bool set_default_time_limits) {
	STACK_UNWINDING_MARK;

	sim::Conver conver;
	conver.setPackagePath(".");
	sim::Conver::ConstructionResult cr =
		conver.constructSimfile(conver_options(set_default_time_limits));

	// Filter out ever line except warnings
	StringView report = conver.getReport();
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

void SipPackage::compile_tex_files() {
	STACK_UNWINDING_MARK;
	THROW("uinmplemented"); // TODO: implement
}

void SipPackage::archive_into_zip(CStringView dest_file) {
	STACK_UNWINDING_MARK;

	stdlog("Zipping...").flush_no_nl();

	// Create zip
	{
		DirectoryChanger dc("..");
		compress_into_zip({dest_file.c_str()}, concat(dest_file, ".zip"));
	}

	stdlog(" done.");
}

void SipPackage::replace_variable_in_configfile(const ConfigFile& cf,
	FilePath configfile_path, StringView configfile_contents,
	StringView var_name, StringView replacement, bool escape_replacement)
{
	STACK_UNWINDING_MARK;

	std::ofstream out(configfile_path.data());
	auto var = cf.getVar(var_name);
	if (var.isSet()) {
		auto var_span = var.value_span;
		out.write(configfile_contents.data(), var_span.beg);
		if (not replacement.empty()) {
			if (escape_replacement)
				out << ConfigFile::escapeString(replacement);
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
				out << ConfigFile::escapeString(replacement);
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
			out << "\n\t" << ConfigFile::escapeString(replacement[0]);
			for (uint i = 1; i < replacement.size(); ++i)
				out << ",\n\t" << ConfigFile::escapeString(replacement[i]);
			out << '\n';
		}
		out << ']';
	};

	auto var = cf.getVar(var_name);
	if (var.isSet()) {
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

	replace_variable_in_configfile(simfile.configFile(), "Simfile",
		simfile_contents, var_name, replacement, escape_replacement);
	simfile_contents = getFileContents("Simfile");
	simfile = sim::Simfile(simfile_contents);
}

void SipPackage::replace_variable_in_simfile(StringView var_name,
	const std::vector<std::string>& replacement)
{
	STACK_UNWINDING_MARK;

	replace_variable_in_configfile(simfile.configFile(), "Simfile",
		simfile_contents, var_name, replacement);
	simfile_contents = getFileContents("Simfile");
	simfile = sim::Simfile(simfile_contents);
}

void SipPackage::replace_variable_in_sipfile(StringView var_name,
	StringView replacement, bool escape_replacement)
{
	STACK_UNWINDING_MARK;

	replace_variable_in_configfile(sipfile.configFile(), "Sipfile",
		sipfile_contents, var_name, replacement, escape_replacement);
	sipfile_contents = getFileContents("Sipfile");
	sipfile = Sipfile(sipfile_contents);
}

void SipPackage::replace_variable_in_sipfile(StringView var_name,
	const std::vector<std::string>& replacement)
{
	STACK_UNWINDING_MARK;

	replace_variable_in_configfile(sipfile.configFile(), "Sipfile",
		sipfile_contents, var_name, replacement);
	sipfile_contents = getFileContents("Sipfile");
	sipfile = Sipfile(sipfile_contents);
}

void SipPackage::create_default_directory_structure() {
	STACK_UNWINDING_MARK;

	stdlog("Creating directories...").flush_no_nl();

	if ((mkdir("check") == -1 and errno != EEXIST)
		or (mkdir("doc") == -1 and errno != EEXIST)
		or (mkdir("in") == -1 and errno != EEXIST)
		or (mkdir("out") == -1 and errno != EEXIST)
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
		"default_time_limit: ", DEFAULT_TIME_LIMIT, "\n"
		"static: [\n"
			"\t# Here provide tests that are \"hard-coded\"\n"
			"\t# Syntax: <test-range>\n"
		"]\ngen: [\n"
			"\t# Here provide rules to generate tests\n"
			"\t# Syntax: <test-range> <generator> [generator arguments]\n"
		"]\n");

	sipfile = Sipfile(default_sipfile_contents);
	putFileContents("Sipfile", default_sipfile_contents);

	stdlog(" done.");
}

void SipPackage::create_default_simfile(Optional<CStringView> problem_name) {
	STACK_UNWINDING_MARK;

	stdlog("Creating Simfile...");

	auto package_dir_filename = filename(intentionalUnsafeStringView(getCWD()).withoutSuffix(1)).to_string();
	if (access("Simfile", F_OK) == 0) {
		simfile_contents = getFileContents("Simfile");
		simfile = sim::Simfile(simfile_contents);

		// Problem name
		auto replace_name_in_simfile = [&](StringView replacement) {
			stdlog("Overwriting the problem name with: ", replacement);
			replace_variable_in_simfile("name", replacement);
		};

		if (problem_name.has_value()) {
			replace_name_in_simfile(problem_name.value());
		} else {
			try { simfile.loadName(); } catch (const std::exception& e) {
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
		if (not simfile.configFile().getVar("memory_limit").isSet())
			replace_variable_in_simfile("memory_limet",
				intentionalUnsafeStringView(toStr(DEFAULT_MEMORY_LIMIT)));

	} else if (problem_name.has_value()) {
		stdlog("name = ", problem_name.value());
		putFileContents("Simfile", intentionalUnsafeStringView(concat(
			"name: ", ConfigFile::escapeString(problem_name.value()),
			"\nmemory_limit: ", DEFAULT_MEMORY_LIMIT, '\n')));

	} else if (not package_dir_filename.empty()) {
		stdlog("name = ", package_dir_filename);
		putFileContents("Simfile", intentionalUnsafeStringView(concat(
			"name: ", ConfigFile::escapeString(package_dir_filename),
			"\nmemory_limit: ", DEFAULT_MEMORY_LIMIT, '\n')));

	} else { // iff absolute path of dir is /
		throw SipError("problem name was neither provided as a command-line"
			" argument nor deduced from the provided package path");
	}
}
