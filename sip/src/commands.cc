#include "commands.hh"
#include "simlib/string_view.hh"
#include "simlib/time.hh"
#include "sip_error.hh"
#include "sip_package.hh"
#include "utils.hh"

#include <limits>
#include <simlib/file_info.hh>
#include <simlib/path.hh>
#include <simlib/process.hh>
#include <simlib/random.hh>
#include <simlib/working_directory.hh>

namespace commands {

// commands.cc
void checker(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	if (args.size() > 0) {
		auto checker = args.extract_next();
		if (not checker.empty() and access(checker, F_OK) != 0) {
			throw SipError("checker: ", checker, " does not exist");
		}

		sp.replace_variable_in_simfile("checker", checker);
	}

	sp.simfile.load_checker();
	stdlog("checker = ", sp.simfile.checker.value_or(""));
}

void clean() {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	sp.clean();
}

void devclean() {
	STACK_UNWINDING_MARK;
	bool recreate_tests_dir = false;
	if (is_directory("tests/")) {
		recreate_tests_dir = true;
		// If tests/ becomes empty, then it will be removed and regenerated
		// tests will be placed inside in/, out/ directories. To prevent it we
		// need to retain this directory
	}

	SipPackage sp;
	sp.remove_generated_test_files();
	sp.clean();

	if (recreate_tests_dir) {
		(void)mkdir("tests/");
	}
}

void doc(ArgvParser args) {
	STACK_UNWINDING_MARK;
	SipPackage::compile_tex_files(args, false);
}

void docwatch(ArgvParser args) {
	STACK_UNWINDING_MARK;
	SipPackage::compile_tex_files(args, true);
}

void genin() {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	sp.generate_test_input_files();
	sp.warn_about_tests_not_specified_as_static_or_generated();
}

void genout() {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	sp.generate_test_output_files();

	if (access("Simfile", F_OK) == 0) {
		sp.save_limits();
	}
	sp.warn_about_tests_not_specified_as_static_or_generated();
}

void gen() {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	sp.generate_test_input_files();

	sp.simfile.load_interactive();
	if (not sp.simfile.interactive) {
		sp.generate_test_output_files();
	}

	if (access("Simfile", F_OK) == 0) {
		sp.save_limits();
	}
	sp.warn_about_tests_not_specified_as_static_or_generated();
}

void regenin() {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	sp.remove_test_files_not_specified_in_sipfile();
	sp.generate_test_input_files();
	sp.warn_about_tests_not_specified_as_static_or_generated();
}

void regen() {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	sp.remove_test_files_not_specified_in_sipfile();
	sp.generate_test_input_files();

	sp.simfile.load_interactive();
	if (not sp.simfile.interactive) {
		sp.generate_test_output_files();
	}

	if (access("Simfile", F_OK) == 0) {
		sp.save_limits();
	}
	sp.warn_about_tests_not_specified_as_static_or_generated();
}

void reseed() {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	try {
		sp.sipfile.load_base_seed(false);
	} catch (...) {
		// Ignore errors
	}

	while (true) {
		using nl = std::numeric_limits<decltype(sp.sipfile.base_seed)>;
		auto new_seed = get_random(nl::min(), nl::max());
		if (new_seed != sp.sipfile.base_seed) {
			sp.sipfile.base_seed = new_seed;
			break;
		}
	}
	sp.replace_variable_in_sipfile(
	   "base_seed",
	   intentional_unsafe_string_view(to_string(sp.sipfile.base_seed)));
}

void help(const char* program_name) {
	STACK_UNWINDING_MARK;

	if (program_name == nullptr) {
		program_name = "sip";
	}

	printf("Usage: %s [options] <command> [<command args>]\n", program_name);
	puts(R"==(Sip is a tool for preparing and managing Sim problem packages

Commands:
  checker [value]       If [value] is specified: set checker to [value].
                          Otherwise print its current value.
  clean                 Prepare package to archiving: remove unnecessary files
                          (compiled programs, latex logs, etc.).
  devclean              Same as command clean, but also remove all generated
                          tests files.
  devzip                Run devclean command and compress the package into zip
                          (named after the current directory with suffix "-dev")
                          within the upper directory.
  doc [pattern...]      Compile latex statements (if there are any) matching
                          [pattern...] (see Patterns section). No pattern means
                          all TeX files.
  docwatch [pattern...] Like command doc, but watch the files for modification
                          and recompile on any change.
  gen                   Generate tests input and output files
  genin                 Generate tests input files
  genout                Generate tests output files using the main solution
  gentests              Alias to command: gen
  help                  Display this information
  init [directory] [name]
                        Initialize Sip package in [directory] (by default
                          current working directory) if [name] is specified, set
                          problem name to it
  interactive [value]   If [value] is specified: set interactive to [value].
                          Otherwise print its current value
  label [value]         If [value] is specified: set label to [value]. Otherwise
                          print its current value
  main-sol [sol]        If [sol] is specified: set main solution to [sol].
                          Otherwise print main solution
  mem [value]           If [value] is specified: set memory limit to
                          [value] MiB. Otherwise print its current value
  name [value]          If [value] is specified: set name to [value]. Otherwise
                          print its current value
  prog [pattern...]     Compile solutions matching [pattern...]
                          (see Patterns section). No pattern means all solutions.
  regen                 Remove test files that don't belong to either static or
                          generated tests. Then generate tests input and output
                          files
  regenin               Remove test files that don't belong to either static or
                          generated tests. Then generate tests input files
  reseed                Changes base_seed in Sipfile, to a new random value.
  save <args...>        Saves args... Allowed args:
                          scoring - saves scoring to Simfile
  statement [value]     If [value] is specified: set statement to [value].
                          Otherwise print its current value
  templ [names...]      Alias to command template names...
  template [names...]   Save templates of names. Available templates:
                          - statement -- saved to doc/statement.tex
                          - checker -- saved to check/checker.cc
                              There are two checker templates: for interactive
                              and non-interactive problems -- it will be chosen based
                              on 'interactive' property from Simfile.
                          - gen -- saved to utils/gen.cc -- Test generator
                              template.
                          Sip will search for templates in
                          ~/.config/sip/templates/ but if specified template is
                          not there, it will use the default one. Template files
                          should have names: statement.tex, checker.cc,
                          interactive_checker.cc. Default templates can be found
                          here: https://github.com/varqox/sip/tree/master/templates
  test [pattern...]     Run solutions matching [pattern...]
                          (see Patterns section) on tests. No pattern means only
                          main solution. Compiles solutions if necessary.
  unset <names...>      Remove variables names... form Simfile e.g.
                          sip unset label interactive
  version               Display version
  zip                   Run clean command and compress the package into zip
                          (named after the current directory) within the upper
                          directory.

Options:
  -C <directory>        Change working directory to <directory> before doing
                          anything
  -h, --help            Display this information
  -V, --version         Display version
  -v, --verbose         Verbose mode
  -q, --quiet           Quiet mode

Patterns:
  If pattern is a path to file, then only this file matches the pattern.
  Otherwise if the pattern does not contain . (dot) sign then it matches file
  if it is a subsequence of the file's path without extension.
  Otherwise patterns matches file if it is a subsequence of the file's path.
  Examples:
    - "foo/abc" matches "foo/abc", "foo/abc.xyz" but not "foo/ab.c"
    - "cc" matches "foo/collect.c", "abcabc.xyz" but not "abc.c" or "xyz.cc"
    - "a.x" matches "foo/abc.xyz" but not "foo/abcxyz.c" or "by.abcxyz"
    - "o.c" matches "foo/x.c" and "bar/o.c"

Sip package tree:
   main/                Main package folder
   |-- check/           Checker folder - holds checker
   |-- doc/             Documents folder - holds problem statement, elaboration,
   |                      etc.
   |-- prog/            Solutions folder - holds solutions
   |-- in/              Tests' input files folder - holds tests input files
   |-- out/             Tests' output files folder - holds tests output files
   |-- utils/           Utilities folder - holds test input generators, input
   |                      verifiers and other files used by Sip
   |-- Simfile          Simfile - holds package primary config
   `-- Sipfile          Sip file - holds Sip configuration and rules for
                          generating test inputs)==");
}

void init(ArgvParser args) {
	STACK_UNWINDING_MARK;

	if (args.size() == 0) {
		throw SipError("missing directory argument - see -h for help");
	}

	auto specified_dir = args.extract_next();
	if (mkdir_r(specified_dir.to_string()) == -1 and errno != EEXIST) {
		throw SipError("failed to create directory: ", specified_dir,
		               " (mkdir_r()", errmsg(), ')');
	}

	if (chdir(specified_dir.data()) == -1) {
		THROW("chdir()", errmsg());
	}

	SipPackage::create_default_directory_structure();
	SipPackage sp;
	sp.create_default_sipfile();

	if (args.size() > 0) {
		sp.create_default_simfile(args.extract_next());
	} else {
		sp.create_default_simfile(std::nullopt);
	}

	name({0, nullptr});
	mem({0, nullptr});
}

void interactive(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	if (args.size() > 0) {
		auto new_interactive = args.extract_next();
		if (not is_one_of(new_interactive, "true", "false")) {
			throw SipError("interactive should have a value of true or false");
		}

		sp.replace_variable_in_simfile("interactive", new_interactive);
	}

	sp.simfile.load_interactive();
	stdlog("interactive = ", sp.simfile.interactive);
}

void label(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	if (args.size() > 0) {
		sp.replace_variable_in_simfile(
		   "label", args.extract_next().operator StringView());
	}

	try {
		sp.simfile.load_label();
		stdlog("label = ", sp.simfile.label.value());
	} catch (const std::exception& e) {
		log_warning(e.what());
	}
}

void main_sol(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	if (args.size() > 0) {
		auto new_main_sol = args.extract_next();
		if (access(new_main_sol, F_OK) != 0) {
			throw SipError("solution: ", new_main_sol, " does not exist");
		}

		auto solutions = sp.simfile.config_file().get_var("solutions");
		if (not solutions.is_set()) {
			sp.replace_variable_in_simfile(
			   "solutions", std::vector<std::string>{new_main_sol.to_string()});
		} else {
			try {
				sp.simfile.load_solutions();
				auto& sols = sp.simfile.solutions;
				auto it = std::find(sols.begin(), sols.end(), new_main_sol);
				if (it != sols.end()) {
					sols.erase(it);
				}

				sols.insert(sols.begin(), new_main_sol.to_string());
				sp.replace_variable_in_simfile("solutions", sols);

			} catch (...) {
				sp.replace_variable_in_simfile(
				   "solutions",
				   std::vector<std::string>{new_main_sol.to_string()});
			}
		}
	}

	sp.simfile.load_solutions();
	if (sp.simfile.solutions.empty()) {
		stdlog("no solution is set");
	} else {
		stdlog("main solution = ", sp.simfile.solutions.front());
	}
}

void mem(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	if (args.size() > 0) {
		auto new_mem_limit = args.extract_next();
		if (not is_digit_not_less_than<1>(new_mem_limit)) {
			throw SipError("memory limit has to be a positive integer");
		}

		sp.replace_variable_in_simfile("memory_limit", new_mem_limit);
	}

	sp.simfile.load_global_memory_limit_only();
	if (not sp.simfile.global_mem_limit.has_value()) {
		stdlog("mem is not set");
	} else {
		stdlog("mem = ", sp.simfile.global_mem_limit.value() >> 20, " MiB");
	}
}

void name(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	if (args.size() > 0) {
		sp.replace_variable_in_simfile(
		   "name", args.extract_next().operator StringView());
	}

	try {
		sp.simfile.load_name();
		stdlog("name = ", sp.simfile.name.value());
	} catch (const std::exception& e) {
		log_warning(e.what());
	}
}

static std::set<StringView> parse_args_to_solutions(const sim::Simfile& simfile,
                                                    ArgvParser args) {
	STACK_UNWINDING_MARK;

	std::set matched_files = files_matching_patterns(args);
	std::set<StringView> choosen_solutions;
	for (auto& solution : simfile.solutions) {
		if (matched_files.find(solution) != matched_files.end()) {
			choosen_solutions.emplace(solution);
		}
	}
	return choosen_solutions;
}

void prog(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	sp.rebuild_full_simfile();

	std::set<StringView> solutions_to_compile;
	if (args.size() > 0) {
		solutions_to_compile = parse_args_to_solutions(sp.full_simfile, args);
	} else {
		for (auto const& sol : sp.full_simfile.solutions) {
			solutions_to_compile.emplace(sol);
		}
	}

	for (auto const& solution : solutions_to_compile) {
		sp.compile_solution(solution);
	}
}

void save(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	if (args.size() == 0) {
		throw SipError("nothing to save");
	}

	while (args.size()) {
		auto arg = args.extract_next();
		if (arg == "scoring") {
			sp.rebuild_full_simfile();
			sp.save_scoring();
		} else {
			throw SipError("save: unrecognized argument ", arg);
		}
	}
}

void statement(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	if (args.size() > 0) {
		auto statement = args.extract_next();
		if (not statement.empty() and access(statement, F_OK) != 0) {
			throw SipError("statement: ", statement, " does not exist");
		}

		sp.replace_variable_in_simfile("statement", statement);
	}

	try {
		sp.simfile.load_statement();
		stdlog("statement = ", sp.simfile.statement.value());
	} catch (const std::exception& e) {
		log_warning(e.what());
	}
}

void template_command(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	while (args.size() > 0) {
		sp.save_template(args.extract_next());
	}
}

void test(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	sp.rebuild_full_simfile();
	if (sp.full_simfile.solutions.empty()) {
		throw SipError("no solution was found");
	}

	std::set<StringView> solutions_to_test;
	if (args.size() > 0) {
		solutions_to_test = parse_args_to_solutions(sp.full_simfile, args);
	} else {
		solutions_to_test.emplace(sp.full_simfile.solutions.front());
	}

	for (auto const& solution : solutions_to_test) {
		sp.judge_solution(solution);
		// Save limits only if Simfile is already created (because if it creates
		// a Simfile without memory limit it causes sip to fail in the next run)
		if (solution == sp.full_simfile.solutions.front() and
		    access("Simfile", F_OK) == 0)
		{
			sp.save_limits();
		}
	}
}

void unset(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	while (args.size() > 0) {
		sp.replace_variable_in_simfile(args.extract_next(), std::nullopt);
	}
}

void zip() {
	STACK_UNWINDING_MARK;
	clean();
	auto cwd = get_cwd();
	--cwd.size;
	SipPackage::archive_into_zip(path_filename(cwd.to_cstr()));
}

void devzip() {
	STACK_UNWINDING_MARK;
	devclean();
	auto cwd = get_cwd();
	--cwd.size;
	cwd.append("-dev");
	SipPackage::archive_into_zip(path_filename(cwd.to_cstr()));
}

} // namespace commands
