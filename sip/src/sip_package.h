#pragma once

#include "sipfile.h"

#include <functional>
#include <simlib/optional.h>
#include <simlib/sim/conver.h>

template<class... Args>
auto log_warning(Args&&... args) {
	return stdlog("\033[1;35mwarning\033[m: ", std::forward<Args>(args)...);
}

class SipPackage {
private:
	class CompilationCache;

public:
	std::string simfile_contents;
	sim::Simfile simfile;

	sim::Simfile full_simfile;

	std::string sipfile_contents;
	Sipfile sipfile;

private:
	// TODO: add description
	void generate_test_out_file();

	// Runs @p callback for each test in @p test_range
	void parse_test_range(StringView test_range,
		const std::function<void(StringView)>& callback);

	// Constructs eligible conver options
	sim::Conver::Options conver_options(bool set_default_time_limits);

public:
	// Initializes the package - loads Simfile (if exists) and Sipfile (if
	// exists)
	SipPackage();

	// Detects all .in files
	void detect_test_in_files();

	// Detects all .out files
	void detect_test_out_files();

	// Generates .in files that have recipe provided in Sipfile
	void generate_test_in_files();

	// For every .in file generates the corresponding .out file
	void generate_test_out_files();

	// Compiles specified solution
	void compile_solution();

	// Compiles all solutions
	void compile_solutions(const std::vector<StringView>& solutions);

	// Compiles checker
	void compile_checker();

	// Removes compiled files, latex logs
	void clean();

	// Removes test files that may be generated
	void remove_generated_test_files();

	// Runs Conver to generate the full_simfile
	void rebuild_full_simfile(bool set_default_time_limits = false);

	// Saves scoring into the Simfile
	void save_scoring();

	// Saves time and memory limits into the Simfile
	void save_limits();

	// Saves the main solution into the Simfile
	void set_main_solution();

	// Compiles all .tex files found in the package
	void compile_tex_files();

	// Archives package contents into the file @p dest_file using .zip
	void archive_into_zip(CStringView dest_file);

	// Replaces or creates variable in the specified config file
	// (deletes variable if @p replacement is empty)
	static void replace_variable_in_configfile(const ConfigFile& cf,
		FilePath configfile_path, StringView configfile_contents,
		StringView var_name, StringView replacement,
		bool escape_replacement = true);

	// Replaces or creates variable in the specified config file
	static void replace_variable_in_configfile(const ConfigFile& cf,
		FilePath configfile_path, StringView configfile_contents,
		StringView var_name, const std::vector<std::string>& replacement);

	// Replaces or creates variable in the Simfile (deletes variable if
	// @p replacement is empty)
	void replace_variable_in_simfile(StringView var_name,
		StringView replacement, bool escape_replacement = true);

	// Replaces or creates variable in the Simfile
	void replace_variable_in_simfile(StringView var_name,
		const std::vector<std::string>& replacement);

	// Replaces or creates variable in the Sipfile (deletes variable if
	// @p replacement is empty)
	void replace_variable_in_sipfile(StringView var_name,
		StringView replacement, bool escape_replacement = true);

	// Replaces or creates variable in the Sipfile
	void replace_variable_in_sipfile(StringView var_name,
		const std::vector<std::string>& replacement);

	// Creates the directory structure of a Sip package in the current directory
	void create_default_directory_structure();

	// Creates the default Sipfile
	void create_default_sipfile();

	// Creates the default Simfile. Then sets name to @p problem_name
	void create_default_simfile(Optional<CStringView> problem_name);
};
