#include "compilation_cache.h"
#include "sip_error.h"
#include "sip_judge_logger.h"
#include "sip_package.h"

#include <fstream>
#include <simlib/filesystem.h>
#include <simlib/process.h>

namespace {
constexpr uint64_t DEFAULT_TIME_LIMIT = 5; // In seconds
constexpr uint64_t DEFAULT_MEMORY_LIMIT = 512; // In MB
} // anonymous namespace

void SipPackage::prepare_judge_worker() {
	STACK_UNWINDING_MARK;

	if (jworker.has_value())
		return;

	jworker = sim::JudgeWorker();
	jworker.value().loadPackage(".", full_simfile.dump());
}

void SipPackage::generate_test_out_file() {
	STACK_UNWINDING_MARK;
	THROW("unimplemented"); // TODO: implement it
}

void SipPackage::parse_test_range(StringView test_range, const std::function<void(StringView)>& callback) {
	STACK_UNWINDING_MARK;
	THROW("unimplemented"); // TODO: implement it
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

void SipPackage::detect_test_in_files() {
	STACK_UNWINDING_MARK;
	THROW("unimplemented"); // TODO: implement it
}

void SipPackage::detect_test_out_files() {
	STACK_UNWINDING_MARK;
	THROW("unimplemented"); // TODO: implement it
}

void SipPackage::generate_test_in_files() {
	STACK_UNWINDING_MARK;
	THROW("unimplemented"); // TODO: implement it
}

void SipPackage::generate_test_out_files() {
	STACK_UNWINDING_MARK;
	THROW("unimplemented"); // TODO: implement it
}

void SipPackage::judge_solution(StringView solution) {
	STACK_UNWINDING_MARK;

	compile_solution(solution);
	compile_checker();
	prepare_judge_worker();

	stdlog('{');
	CompilationCache::load_solution(jworker.value(), solution);
	CompilationCache::load_checker(jworker.value());
	auto jrep1 = jworker.value().judge(false, SipJudgeLogger());
	auto jrep2 = jworker.value().judge(true, SipJudgeLogger());
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
	THROW("uinmplemented"); // TODO: implement
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

	// Check Sipfile
	if (access("Sipfile", F_OK) == 0) {
		sipfile.loadDefaultTimeLimit();
		copts.max_time_limit = sipfile.default_time_limit;
	} else {
		copts.max_time_limit = DEFAULT_TIME_LIMIT * 1'000'000;
	}

	return copts;
}

void SipPackage::rebuild_full_simfile(bool set_default_time_limits) {
	STACK_UNWINDING_MARK;

	sim::Conver conver;
	conver.setPackagePath(".");
	sim::Conver::ConstructionResult cr =
		conver.constructSimfile(conver_options(set_default_time_limits));

	// TODO: instead false verbose was here - remove this if or resurrect verbose in some way...
	if (false) {
		stdlog(conver.getReport());
	} else {
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
	}

	// Ignore the need for the model solution to set the time limits - the
	// time limits will be set to max_time_limit
	// throw_assert(cr.status == sim::Conver::Status::COMPLETE);

	full_simfile = std::move(cr.simfile);
	// jworker has become outdated
	jworker = std::nullopt;
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

void SipPackage::set_main_solution() {
	STACK_UNWINDING_MARK;
	THROW("uinmplemented"); // TODO: implement
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
