#include "commands.h"
#include "sip_error.h"
#include "sip_package.h"
#include "utils.h"

#include <simlib/filesystem.h>
#include <simlib/libarchive_zip.h>
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

	sp.simfile.loadChecker();
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

	if (access("Simfile", F_OK) == 0)
		sp.save_scoring();
}

void genout(ArgvParser) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	sp.generate_test_out_files();

	if (access("Simfile", F_OK) == 0) {
		sp.save_scoring();
		sp.save_limits();
	}
}

void gen(ArgvParser) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	sp.generate_test_in_files();
	sp.generate_test_out_files();

	if (access("Simfile", F_OK) == 0) {
		sp.save_scoring();
		sp.save_limits();
	}
}

void regenin(ArgvParser) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	sp.remove_test_files_not_specified_in_sipfile();
	sp.generate_test_in_files();

	if (access("Simfile", F_OK) == 0)
		sp.save_scoring();
}

void regen(ArgvParser) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	sp.remove_test_files_not_specified_in_sipfile();
	sp.generate_test_in_files();
	sp.generate_test_out_files();

	if (access("Simfile", F_OK) == 0) {
		sp.save_scoring();
		sp.save_limits();
	}
}

void help(const char* program_name) {
	STACK_UNWINDING_MARK;

	if (program_name == NULL)
		program_name = "sip";

	printf("Usage: %s [options] <command> [<command args>]\n", program_name);
	puts("Sip is a tool for preparing and managing Sim problem packages");
	puts("");
	puts("Commands:");
	puts("  checker [value]       If [value] is specified: set checker to [value]. Otherwise print its current value");
	puts("  clean [arg...]        Prepare package to archiving: remove unnecessary files (compiled programs, latex logs, etc.).");
	puts("                        Allowed args:");
	puts("                          tests - remove all generated tests files");
	puts("  doc [watch]           Compile latex statements (if there are any). If watch is specified as an argument, then all statement files will be watched and recompiled on any change");
	puts("  gen                   Generate tests input and output files");
	puts("  genin                 Generate tests input files");
	puts("  genout                Generate tests output files using the main solution");
	puts("  gentests              Alias to command: gen");
	puts("  init [directory] [name]");
	puts("                        Initialize Sip package in [directory] (by default current working directory) if [name] is specified, set problem name to it");
	puts("  label [value]         If [value] is specified: set label to [value]. Otherwise print its current value");
	puts("  main-sol [sol]        If [sol] is specified: set main solution to [sol]. Otherwise print main solution");
	puts("  mem [value]           If [value] is specified: set memory limit to [value] MB. Otherwise print its current value");
	puts("  name [value]          If [value] is specified: set name to [value]. Otherwise print its current value");
	puts("  prog [sol...]         Compile solutions [sol...] (all solutions by default). [sol] has the same meaning as in command 'test'");
	puts("  regen                 Remove test files that don't belong to either static or generated tests. Then generate tests input and output files");
	puts("  regenin               Remove test files that don't belong to either static or generated tests. Then generate tests input files");
	puts("  statement [value]     If [value] is specified: set statement to [value]. Otherwise print its current value");
	puts("  test [sol...]         Run solutions [sol...] on tests (only main solution by default) (compile solutions if necessary). If [sol] is a path to a solution then it is used, otherwise all solutions that have [sol] as a subsequence are used.");
	// puts("  verify [sol...]       Run inver and solutions [sol...] on tests (all solutions by default) (compiles solutions if necessary)");
	puts("  zip [clean args...]   Run clean command with [clean args] and compress the package into zip (named after the current directory) within the upper directory.");
	puts("");
	puts("Options:");
	puts("  -C <directory>        Change working directory to <directory> before doing anything");
	puts("  -h, --help            Display this information");
	puts("  -q, --quiet           Quiet mode");
	// puts("  -v, --verbose          Verbose mode (the default option)");
	puts("");
	puts("Sip package tree:");
	puts("   main/                Main package folder");
	puts("   |-- check/           Checker folder - holds checker");
	puts("   |-- doc/             Documents folder - holds problem statement, elaboration, etc.");
	puts("   |-- prog/            Solutions folder - holds solutions");
	puts("   |-- in/              Tests' input files folder - holds tests input files");
	puts("   |-- out/             Tests' output files folder - holds tests output files");
	puts("   |-- utils/           Utilities folder - holds test input generators, input verifiers and other files used by Sip");
	puts("   |-- Simfile          Simfile - holds package primary config");
	puts("   `-- Sipfile          Sip file - holds Sip configuration and rules for generating test inputs");
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

void label(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	if (args.size() > 0)
		sp.replace_variable_in_simfile("label", args.extract_next());

	sp.simfile.loadLabel();
	stdlog("label = ", sp.simfile.label);
}

void main_sol(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	if (args.size() > 0) {
		auto new_main_sol = args.extract_next();
		if (access(new_main_sol, F_OK) != 0)
			throw SipError("file: ", new_main_sol, " does not exist");

		auto solutions = sp.simfile.configFile().getVar("solutions");
		if (not solutions.isSet()) {
			sp.replace_variable_in_simfile("solutions",
				std::vector<std::string>{new_main_sol.to_string()});
		} else {
			try {
				sp.simfile.loadSolutions();
				auto& sols = sp.simfile.solutions;
				auto it = std::find(sols.begin(), sols.end(), new_main_sol);
				if (it != sols.end())
					sols.erase(it);

				sols.insert(sols.begin(), new_main_sol.to_string());
				sp.replace_variable_in_simfile("solutions", sols);

			} catch (...) {
				sp.replace_variable_in_simfile("solutions",
					std::vector<std::string>{new_main_sol.to_string()});
			}
		}
	}

	sp.simfile.loadSolutions();
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

	sp.simfile.loadGlobalMemoryLimitOnly();
	stdlog("mem = ", sp.simfile.global_mem_limit >> 20);
}

void name(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	if (args.size() > 0)
		sp.replace_variable_in_simfile("name", args.extract_next());

	sp.simfile.loadName();
	stdlog("name = ", sp.simfile.name);
}

static AVLDictSet<StringView> parse_args_to_solutions(
	const sim::Simfile& simfile, ArgvParser args)
{
	if (args.size() == 0)
		return {};

	AVLDictSet<StringView> choosen_solutions;
	do {
		auto arg = args.extract_next();
		// If a path to solution was provided then choose it
		auto it = std::find(simfile.solutions.begin(), simfile.solutions.end(), arg);
		if (it != simfile.solutions.end()) {
			choosen_solutions.emplace(*it);
		// There is no solution with path equal to the provided path, so
		// choose all that contain arg as a subsequence
		} else {
			throw_assert(is_subsequence("abc", "a b c"));

			for (auto const& solution : simfile.solutions)
				if (is_subsequence(arg, solution))
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

	solutions_to_compile.for_each([&](StringView solution) {
		sp.compile_solution(solution);
	});
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

	sp.simfile.loadStatement();
	stdlog("statement = ", sp.simfile.statement);
}

void test(ArgvParser args) {
	STACK_UNWINDING_MARK;

	SipPackage sp;
	sp.rebuild_full_simfile();
	// Save scoring only if Simfile is already created (because if it creates a Simfile without memory limit it causes sip to fail in the next run)
	if (access("Simfile", F_OK) == 0)
		sp.save_scoring();

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
			access("Simfile", F_OK) == 0)
		{
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
