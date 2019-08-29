#include "commands.h"
#include "sip_error.h"
#include "sip_package.h"
#include "utils.h"

#include <simlib/filesystem.h>
#include <simlib/process.h>

namespace commands {

// commands.cc
void checker(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	if (args.size() > 0) {
		auto checker = args.extract_next();
		if (not checker.empty() and access(checker, F_OK) != 0)
			throw SipError("file: ", checker, " does not exist");

		sp.replace_variable_in_simfile("checker", checker);
	}

	sp.simfile.load_checker();
	stdlog("checker = ", sp.simfile.checker.value_or(""));
}

void clean(ArgvParser args) {
	STACK_UNWINDING_MARK;
	SipPackage sp;
	sp.clean();

	while (args.size() > 0) {
		auto arg = args.extract_next();
		if (arg == "tests")
			sp.remove_generated_test_files();
		else
			throw SipError("clean: unrecognized argument: ", arg);
	}
}

void doc(ArgvParser args) {
	STACK_UNWINDING_MARK;

	bool watch = false;
	while (args.size() > 0) {
		auto arg = args.extract_next();
		if (arg == "watch")
			watch = true;
		else
			throw SipError("clean: unrecognized argument: ", arg);
	}

	SipPackage sp;
	sp.compile_tex_files(watch);
}

void genin(ArgvParser) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	sp.generate_test_in_files();
}

void genout(ArgvParser) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	sp.generate_test_out_files();

	if (access("Simfile", F_OK) == 0)
		sp.save_limits();
}

void gen(ArgvParser) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	sp.generate_test_in_files();

	sp.simfile.load_interactive();
	if (not sp.simfile.interactive)
		sp.generate_test_out_files();

	if (access("Simfile", F_OK) == 0)
		sp.save_limits();
}

void regenin(ArgvParser) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	sp.remove_test_files_not_specified_in_sipfile();
	sp.generate_test_in_files();
}

void regen(ArgvParser) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	sp.remove_test_files_not_specified_in_sipfile();
	sp.generate_test_in_files();

	sp.simfile.load_interactive();
	if (not sp.simfile.interactive)
		sp.generate_test_out_files();

	if (access("Simfile", F_OK) == 0)
		sp.save_limits();
}

void help(const char* program_name) {
	STACK_UNWINDING_MARK;

	if (program_name == nullptr)
		program_name = "sip";

	printf("Usage: %s [options] <command> [<command args>]\n", program_name);
	puts(R"==(Sip is a tool for preparing and managing Sim problem packages

Commands:
  checker [value]       If [value] is specified: set checker to [value].
                          Otherwise print its current value
  clean [arg...]        Prepare package to archiving: remove unnecessary files
                          (compiled programs, latex logs, etc.).
                        Allowed args:
                          tests - remove all generated tests files
  doc [watch]           Compile latex statements (if there are any). If watch is
                          specified as an argument, then all statement files
                          will be watched and recompiled on any change
  gen                   Generate tests input and output files
  genin                 Generate tests input files
  genout                Generate tests output files using the main solution
  gentests              Alias to command: gen
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
  prog [sol...]         Compile solutions [sol...] (all solutions by default).
                          [sol] has the same meaning as in command 'test'
  regen                 Remove test files that don't belong to either static or
                          generated tests. Then generate tests input and output
                          files
  regenin               Remove test files that don't belong to either static or
                          generated tests. Then generate tests input files
  save <args...>        Saves args... Allowed args:
                          scoring - saves scoring to Simfile
  statement [value]     If [value] is specified: set statement to [value].
                          Otherwise print its current value
  test [sol...]         Run solutions [sol...] on tests (only main solution by
                          default) (compile solutions if necessary). If [sol] is
                          a path to a solution then it is used, otherwise all
                          solutions that have [sol] as a subsequence are used.
  zip [clean args...]   Run clean command with [clean args] and compress the
                          package into zip (named after the current directory)
                          within the upper directory.

Options:
  -C <directory>        Change working directory to <directory> before doing
                          anything
  -h, --help            Display this information
  -q, --quiet           Quiet mode

Sip package tree:
   main/                Main package folder
   |-- check/           Checker folder - holds checker
   |-- doc/             Documents folder - holds problem statement, elaboration,
                          etc.
   |-- prog/            Solutions folder - holds solutions
   |-- in/              Tests' input files folder - holds tests input files
   |-- out/             Tests' output files folder - holds tests output files
   |-- utils/           Utilities folder - holds test input generators, input
                          verifiers and other files used by Sip
   |-- Simfile          Simfile - holds package primary config
   `-- Sipfile          Sip file - holds Sip configuration and rules for
                          generating test inputs)==");
}

void init(ArgvParser args) {
	STACK_UNWINDING_MARK;

	if (args.size() == 0)
		throw SipError("missing directory argument - see -h for help");

	auto specified_dir = args.extract_next();
	if (mkdir_r(specified_dir.to_string()) == -1 and errno != EEXIST)
		throw SipError("failed to create directory: ", specified_dir,
		               "(mkdir_r()", errmsg(), ')');

	if (chdir(specified_dir.data()) == -1)
		THROW("chdir()", errmsg());

	SipPackage sp;
	sp.create_default_directory_structure();
	sp.create_default_sipfile();

	if (args.size() > 0)
		sp.create_default_simfile(args.extract_next());
	else
		sp.create_default_simfile(std::nullopt);
}

void interactive(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	if (args.size() > 0) {
		auto new_interactive = args.extract_next();
		if (not is_one_of(new_interactive, "true", "false")) {
			throw SipError(
			   "interactive has to be either \"true\" or \"false\"");
		}

		sp.replace_variable_in_simfile("interactive", new_interactive);
	}

	sp.simfile.load_interactive();
	stdlog("interactive = ", sp.simfile.interactive);
}

void label(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	if (args.size() > 0)
		sp.replace_variable_in_simfile("label", args.extract_next());

	sp.simfile.load_label();
	stdlog("label = ", sp.simfile.label);
}

void main_sol(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	if (args.size() > 0) {
		auto new_main_sol = args.extract_next();
		if (access(new_main_sol, F_OK) != 0)
			throw SipError("file: ", new_main_sol, " does not exist");

		auto solutions = sp.simfile.config_file().get_var("solutions");
		if (not solutions.is_set()) {
			sp.replace_variable_in_simfile(
			   "solutions",
			   std::vector<std::string> {new_main_sol.to_string()});
		} else {
			try {
				sp.simfile.load_solutions();
				auto& sols = sp.simfile.solutions;
				auto it = std::find(sols.begin(), sols.end(), new_main_sol);
				if (it != sols.end())
					sols.erase(it);

				sols.insert(sols.begin(), new_main_sol.to_string());
				sp.replace_variable_in_simfile("solutions", sols);

			} catch (...) {
				sp.replace_variable_in_simfile(
				   "solutions",
				   std::vector<std::string> {new_main_sol.to_string()});
			}
		}
	}

	sp.simfile.load_solutions();
	if (sp.simfile.solutions.empty())
		stdlog("no solution is set");
	else
		stdlog("main solution = ", sp.simfile.solutions.front());
}

void mem(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	if (args.size() > 0) {
		auto new_mem_limit = args.extract_next();
		if (not isDigitNotLessThan<1>(new_mem_limit))
			throw SipError("memory limit has to be a positive integer");

		sp.replace_variable_in_simfile("memory_limit", new_mem_limit);
	}

	sp.simfile.load_global_memory_limit_only();
	if (not sp.simfile.global_mem_limit.has_value())
		stdlog("mem is not set");
	else
		stdlog("mem = ", sp.simfile.global_mem_limit.value() >> 20);
}

void name(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	if (args.size() > 0)
		sp.replace_variable_in_simfile("name", args.extract_next());

	sp.simfile.load_name();
	stdlog("name = ", sp.simfile.name);
}

static AVLDictSet<StringView>
parse_args_to_solutions(const sim::Simfile& simfile, ArgvParser args) {
	STACK_UNWINDING_MARK;

	if (args.size() == 0)
		return {};

	AVLDictSet<StringView> choosen_solutions;
	do {
		auto arg = args.extract_next();
		// If a path to solution was provided then choose it
		auto it =
		   std::find(simfile.solutions.begin(), simfile.solutions.end(), arg);
		if (it != simfile.solutions.end()) {
			choosen_solutions.emplace(*it);
		} else {
			// There is no solution with path equal to the provided path, so
			// choose all that contain arg as a subsequence
			for (auto const& solution : simfile.solutions)
				if (matches_pattern(arg, solution))
					choosen_solutions.emplace(solution);
		}
	} while (args.size() > 0);

	return choosen_solutions;
}

void prog(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	sp.rebuild_full_simfile();

	AVLDictSet<StringView> solutions_to_compile;
	if (args.size() > 0) {
		solutions_to_compile = parse_args_to_solutions(sp.full_simfile, args);
	} else {
		for (auto const& sol : sp.full_simfile.solutions)
			solutions_to_compile.emplace(sol);
	}

	solutions_to_compile.for_each(
	   [&](StringView solution) { sp.compile_solution(solution); });
}

void save(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	if (args.size() == 0)
		throw SipError("nothing to save");

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
		if (not statement.empty() and access(statement, F_OK) != 0)
			throw SipError("file: ", statement, " does not exist");

		sp.replace_variable_in_simfile("statement", statement);
	}

	sp.simfile.load_statement();
	stdlog("statement = ", sp.simfile.statement);
}

void test(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	sp.rebuild_full_simfile();
	if (sp.full_simfile.solutions.empty())
		throw SipError("no solution was found");

	AVLDictSet<StringView> solutions_to_test;
	if (args.size() > 0)
		solutions_to_test = parse_args_to_solutions(sp.full_simfile, args);
	else
		solutions_to_test.emplace(sp.full_simfile.solutions.front());

	solutions_to_test.for_each([&](StringView solution) {
		sp.judge_solution(solution);
		// Save limits only if Simfile is already created (because if it creates
		// a Simfile without memory limit it causes sip to fail in the next run)
		if (solution == sp.full_simfile.solutions.front() and
		    access("Simfile", F_OK) == 0) {
			sp.save_limits();
		}
	});
}

void zip(ArgvParser args) {
	STACK_UNWINDING_MARK;

	clean(args);
	auto cwd = getCWD();
	--cwd.size;
	SipPackage().archive_into_zip(filename(cwd.to_cstr()));
}

} // namespace commands
