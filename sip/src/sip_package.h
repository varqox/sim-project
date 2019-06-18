#pragma once

#include "sip_judge_logger.h"
#include "sipfile.h"
#include "tests_files.h"

#include <functional>
#include <simlib/sim/conver.h>

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
	Optional<sim::JudgeWorker> jworker;
	Optional<TestsFiles> tests_files;

	// Loads the default time limit from Sipfile or return the default value.
	// Returned value represents time in microseconds
	std::chrono::nanoseconds get_default_time_limit();

	// Detects all .in files and .out files
	void prepare_tests_files();

	// Prepares jworker
	void prepare_judge_worker();

	// Runs @p test.generator and places its output in the @p in_file
	void generate_test_in_file(const Sipfile::GenTest& test,
	                           CStringView in_file);

	// Runs model solution on @p test.in and places the output in the
	// @p test.out file
	sim::JudgeReport::Test
	generate_test_out_file(const sim::Simfile::Test& test,
	                       SipJudgeLogger& logger);

	// Runs @p callback for each test in @p test_range
	void parse_test_range(StringView test_range,
	                      const std::function<void(StringView)>& callback);

	// Constructs eligible conver options
	sim::Conver::Options conver_options(bool set_default_time_limits);

	void reload_simfile_from_str(std::string contents);

	void reload_sipfile_from_str(std::string contents);

public:
	// Initializes the package - loads Simfile (if exists) and Sipfile (if
	// exists)
	SipPackage();

	// Generates .in files that have recipe provided in Sipfile
	void generate_test_in_files();

	// Removes tests from "limits" variable in Simfile that have no
	// corresponding input file
	void remove_tests_with_no_in_file_from_limits_in_simfile();

	// For every .in file generates the corresponding .out file
	void generate_test_out_files();

	// Runs specified solution on all tests
	void judge_solution(StringView solution);

	// Compiles specified solution
	void compile_solution(StringView solution);

	// Compiles checker
	void compile_checker();

	// Removes compiled files, latex logs
	void clean();

	// Removes test files that may be generated
	void remove_generated_test_files();

	// Removes tests files that are not specified as generated or static
	void remove_test_files_not_specified_in_sipfile();

	// Runs Conver to generate the full_simfile
	void rebuild_full_simfile(bool set_default_time_limits = false);

	// Saves scoring into the Simfile
	void save_scoring();

	// Saves time and memory limits into the Simfile
	void save_limits();

	// Compiles all .tex files found in the package, if watch is true, then
	// every .tex file will be recompiled on any change
	void compile_tex_files(bool watch);

	// Archives package contents into the file @p dest_file using .zip
	void archive_into_zip(CStringView dest_file);

	// Replaces or creates variable in the specified config file
	// (deletes variable if @p replacement is empty)
	static void replace_variable_in_configfile(const ConfigFile& cf,
	                                           FilePath configfile_path,
	                                           StringView configfile_contents,
	                                           StringView var_name,
	                                           StringView replacement,
	                                           bool escape_replacement = true);

	// Replaces or creates variable in the specified config file
	static void replace_variable_in_configfile(
	   const ConfigFile& cf, FilePath configfile_path,
	   StringView configfile_contents, StringView var_name,
	   const std::vector<std::string>& replacement);

	// Replaces or creates variable in the Simfile (deletes variable if
	// @p replacement is empty)
	void replace_variable_in_simfile(StringView var_name,
	                                 StringView replacement,
	                                 bool escape_replacement = true);

	// Replaces or creates variable in the Simfile
	void
	replace_variable_in_simfile(StringView var_name,
	                            const std::vector<std::string>& replacement);

	// Replaces or creates variable in the Sipfile (deletes variable if
	// @p replacement is empty)
	void replace_variable_in_sipfile(StringView var_name,
	                                 StringView replacement,
	                                 bool escape_replacement = true);

	// Replaces or creates variable in the Sipfile
	void
	replace_variable_in_sipfile(StringView var_name,
	                            const std::vector<std::string>& replacement);

	// Creates the directory structure of a Sip package in the current directory
	void create_default_directory_structure();

	// Creates the default Sipfile
	void create_default_sipfile();

	// Creates the default Simfile. Then sets name to @p problem_name
	void create_default_simfile(Optional<CStringView> problem_name);
};
