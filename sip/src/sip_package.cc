#include "compilation_cache.h"
#include "sip_package.h"

#include <simlib/filesystem.h>
#include <simlib/process.h>
#include <fstream>

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

void SipPackage::compile_solution() {
	STACK_UNWINDING_MARK;
	THROW("unimplemented"); // TODO: implement it
}

void SipPackage::compile_solutions() {
	STACK_UNWINDING_MARK;
	THROW("unimplemented"); // TODO: implement it
}

void SipPackage::compile_checker() {
	STACK_UNWINDING_MARK;
	THROW("unimplemented"); // TODO: implement it
}

void SipPackage::clean() {
	STACK_UNWINDING_MARK;

	auto tmplog = stdlog("Cleaning...");
	tmplog.flush_no_nl();

	CompilationCache::clear();
	if (remove_r("utils/latex/") and errno != ENOENT)
		THROW("remove_r()", errmsg());

	tmplog(" done.");
}

void SipPackage::remove_generated_test_files() {
	STACK_UNWINDING_MARK;
	THROW("uinmplemented"); // TODO: implement
}

void SipPackage::rebuild_simfile() {
	STACK_UNWINDING_MARK;
	THROW("uinmplemented"); // TODO: implement
}

void SipPackage::save_scoring() {
	STACK_UNWINDING_MARK;
	THROW("uinmplemented"); // TODO: implement
}

void SipPackage::save_limits() {
	STACK_UNWINDING_MARK;
	THROW("uinmplemented"); // TODO: implement
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

	auto tmplog = stdlog("Zipping...");
	tmplog.flush_no_nl();

	// Create zip
	{
		DirectoryChanger dc("..");
		compress_into_zip({dest_file.c_str()}, concat(dest_file, ".zip"));
	}

	tmplog(" done.");
}

void SipPackage::replace_variable_in_configfile(const ConfigFile& cf,
	FilePath configfile_path, StringView configfile_contents,
	StringView var_name, StringView replacement, bool escape_replacement)
{
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
	replace_variable_in_configfile(simfile.configFile(), "Simfile",
		simfile_contents, var_name, replacement, escape_replacement);
	simfile_contents = getFileContents("Simfile");
	simfile = sim::Simfile(simfile_contents);
}

void SipPackage::replace_variable_in_simfile(StringView var_name,
	const std::vector<std::string>& replacement)
{
	replace_variable_in_configfile(simfile.configFile(), "Simfile",
		simfile_contents, var_name, replacement);
	simfile_contents = getFileContents("Simfile");
	simfile = sim::Simfile(simfile_contents);
}

void SipPackage::replace_variable_in_sipfile(StringView var_name,
	StringView replacement, bool escape_replacement)
{
	replace_variable_in_configfile(sipfile.configFile(), "Sipfile",
		sipfile_contents, var_name, replacement, escape_replacement);
	sipfile_contents = getFileContents("Sipfile");
	sipfile = Sipfile(sipfile_contents);
}

void SipPackage::replace_variable_in_sipfile(StringView var_name,
	const std::vector<std::string>& replacement)
{
	replace_variable_in_configfile(sipfile.configFile(), "Sipfile",
		sipfile_contents, var_name, replacement);
	sipfile_contents = getFileContents("Sipfile");
	sipfile = Sipfile(sipfile_contents);
}

void SipPackage::create_default_directory_structure() {
	STACK_UNWINDING_MARK;

	auto tmplog = stdlog("Creating directories...");
	tmplog.flush_no_nl();

	if ((mkdir("check") == -1 and errno != EEXIST)
		or (mkdir("doc") == -1 and errno != EEXIST)
		or (mkdir("in") == -1 and errno != EEXIST)
		or (mkdir("out") == -1 and errno != EEXIST)
		or (mkdir("prog") == -1 and errno != EEXIST)
		or (mkdir("utils") == -1 and errno != EEXIST))
	{
		THROW("mkdir()", errmsg());
	}

	tmplog(" done.");
}

void SipPackage::create_default_sipfile() {
	STACK_UNWINDING_MARK;

	if (access("Sipfile", F_OK) == 0) {
		stdlog("Sipfile already created");
		return;
	}

	auto tmplog = stdlog("Creating Sipfile...");
	tmplog.flush_no_nl();

	constexpr const char* DEFAULT_SIPFILE_CONTENTS =
		"default_time_limit: 5\n"
		"static: [\n"
			"\t# Here provide tests that are \"hard-coded\"\n"
			"\t# Syntax: <test-range>\n"
		"]\ngen: [\n"
			"\t# Here provide rules to generate tests\n"
			"\t# Syntax: <test-range> <generator> [generator arguments]\n"
		"]\n";

	sipfile = Sipfile(DEFAULT_SIPFILE_CONTENTS);
	putFileContents("Sipfile", DEFAULT_SIPFILE_CONTENTS);

	tmplog(" done.");
}

void SipPackage::create_default_simfile(Optional<CStringView> problem_name) {
	STACK_UNWINDING_MARK;

	stdlog("Creating Simfile...");

	auto package_dir_filename = filename(intentionalUnsafeStringView(getCWD()).withoutSuffix(1)).to_string();
	if (access("Simfile", F_OK) == 0) {
		simfile_contents = getFileContents("Simfile");
		simfile = sim::Simfile(simfile_contents);

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
	} else if (problem_name.has_value()) {
		stdlog("name = ", problem_name.value());
		putFileContents("Simfile", intentionalUnsafeStringView(concat("name: ",
			ConfigFile::escapeString(problem_name.value()), '\n')));
	} else if (not package_dir_filename.empty()) {
		stdlog("name = ", package_dir_filename);
		putFileContents("Simfile", intentionalUnsafeStringView(concat("name: ",
			ConfigFile::escapeString(package_dir_filename), '\n')));
	} else { // iff absolute path of dir is /
		throw SipError("problem name was neither provided as a command-line"
			" argument nor deduced from the provided package path");
	}
}
