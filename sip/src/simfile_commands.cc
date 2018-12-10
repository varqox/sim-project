#include "commands.h"
#include "sip.h"

#include <fstream>
#include <simlib/filesystem.h>
#include <simlib/process.h>

using sim::Simfile;
using std::string;
using std::vector;

void replace_var_in_simfile(const Simfile& sf, FilePath simfile_path,
	StringView simfile_contents, StringView var_name, StringView replacement,
	bool escape_replacement)
{
	std::ofstream out(simfile_path.data());
	auto var = sf.configFile().getVar(var_name);
	if (var.isSet()) {
		auto var_span = var.value_span;
		out.write(simfile_contents.data(), var_span.beg);
		if (not replacement.empty()) {
			if (escape_replacement)
				out << ConfigFile::escapeString(replacement);
			else
				out.write(replacement.data(), replacement.size());
		}
		out.write(simfile_contents.data() + var_span.end,
			simfile_contents.size() - var_span.end);

	} else {
		out.write(simfile_contents.data(), simfile_contents.size());
		if (not simfile_contents.empty() and simfile_contents.back() != '\n')
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

void replace_var_in_simfile(const Simfile& sf, FilePath simfile_path,
	StringView simfile_contents, StringView var_name,
	const vector<string>& replacement)
{
	std::ofstream out(simfile_path.data());
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

	auto var = sf.configFile().getVar(var_name);
	if (var.isSet()) {
		auto var_span = var.value_span;
		out.write(simfile_contents.data(), var_span.beg);
		print_replacement();
		out.write(simfile_contents.data() + var_span.end,
			simfile_contents.size() - var_span.end);

	} else {
		out.write(simfile_contents.data(), simfile_contents.size());
		if (simfile_contents.back() != '\n')
			out << '\n';

		out << var_name << ": ";
		print_replacement();
		out << '\n';
	}
}

namespace command {

int init(ArgvParser args) {
	STACK_UNWINDING_MARK;

	if (args.size() == 0) {
		errlog("Missing directory argument - see -h for help");
		return 1;
	}

	InplaceBuff<32> dir;
	dir.append(args.extract_next());
	if (dir.back() != '/')
		dir.append('/');

	stdlog("Creating directories...");
	if ((mkdir_r(dir.to_string()) == -1 && errno != EEXIST)
		|| (mkdir(concat(dir, "check")) == -1 && errno != EEXIST)
		|| (mkdir(concat(dir, "doc")) == -1 && errno != EEXIST)
		|| (mkdir(concat(dir, "in")) == -1 && errno != EEXIST)
		|| (mkdir(concat(dir, "out")) == -1 && errno != EEXIST)
		|| (mkdir(concat(dir, "prog")) == -1 && errno != EEXIST)
		|| (mkdir(concat(dir, "utils")) == -1 && errno != EEXIST))
	{
		THROW("mkdir()", errmsg());
	}

	auto absoluted_dir = abspath(dir, getCWD().to_string());
	StringView dir_filename = filename(StringView(absoluted_dir).withoutSuffix(1));

	Simfile sf;
	auto simfile_path = concat(dir, "Simfile");
	string simfile_contents;
	if (access(simfile_path, F_OK) == 0) {
		simfile_contents = getFileContents(simfile_path);
		sf = Simfile(simfile_contents);

		auto replace_name_in_simfile = [&](StringView replacement) {
			stdlog("Overwriting the problem name with: ", replacement);
			replace_var_in_simfile(sf, simfile_path, simfile_contents,
				"name", replacement);
		};

		if (args.size() > 0) { // Name to set was provided as command line parameter
			replace_name_in_simfile(args.extract_next());
		} else {
			try { sf.loadName(); } catch (const std::exception& e) {
				stdlog("\033[1;35mwarning\033[m: ignoring: ", e.what());

				if (not dir_filename.empty()) {
					replace_name_in_simfile(dir_filename);
				} else { // iff absolute path of dir is /
					errlog("Error: problem name was neither found in Simfile"
						", nor provided as a command-line argument"
						" nor deduced from the provided package path");
					return 1;
				}
			}
		}
	} else if (args.size() > 0) {
		stdlog("name = ", args.next());
		putFileContents(simfile_path, intentionalUnsafeStringView(concat("name: ",
			ConfigFile::escapeString(args.extract_next()), '\n')));
	} else if (not dir_filename.empty()) {
		stdlog("name = ", dir_filename);
		putFileContents(simfile_path, intentionalUnsafeStringView(concat("name: ",
			ConfigFile::escapeString(dir_filename), '\n')));
	} else { // iff absolute path of dir is /
		errlog("Error: problem name was neither provided as a command-line"
			" argument nor deduced from the provided package path");
		return 1;
	}

	auto sipfile_path = concat(dir, "Sipfile");
	if (access(sipfile_path, F_OK) != 0)
		putFileContents(sipfile_path,
			"default_time_limit: 5\n"
			"static: [\n"
				"\t# Here provide tests that are \"hard-coded\"\n"
				"\t# Syntax: <test-range>\n"
			"]\ngen: [\n"
				"\t# Here provide rules to generate tests\n"
				"\t# Syntax: <test-range> <generator> [generator arguments]\n"
			"]\n");

	return 0;
}

int name(ArgvParser args) {
	STACK_UNWINDING_MARK;

	if (access("Simfile", F_OK) != 0) {
		errlog("Error: Simfile is missing. If you are sure that you are in the"
			" correct directory, running: `sip init . <name>` may help.");
		return 1;
	}

	auto simfile_contents = getFileContents("Simfile");
	Simfile sf = Simfile(simfile_contents);
	if (args.size() > 0) {
		replace_var_in_simfile(sf, "Simfile", simfile_contents, "name", args.next());
		stdlog("name = ", args.next());
	} else {
		sf.loadName();
		stdlog("name = ", sf.name);
	}

	return 0;
}

int label(ArgvParser args) {
	STACK_UNWINDING_MARK;

	if (access("Simfile", F_OK) != 0) {
		errlog("Error: Simfile is missing. If you are sure that you are in the"
			" correct directory, running: `sip init . <name>` may help.");
		return 1;
	}

	auto simfile_contents = getFileContents("Simfile");
	Simfile sf = Simfile(simfile_contents);
	if (args.size() > 0) {
		replace_var_in_simfile(sf, "Simfile", simfile_contents, "label", args.next());
		stdlog("label = ", args.next());
	} else {
		sf.loadLabel();
		stdlog("label = ", sf.label);
	}

	return 0;
}

int checker(ArgvParser args) {
	STACK_UNWINDING_MARK;

	if (access("Simfile", F_OK) != 0) {
		errlog("Error: Simfile is missing. If you are sure that you are in the"
			" correct directory, running: `sip init . <name>` may help.");
		return 1;
	}

	auto simfile_contents = getFileContents("Simfile");
	Simfile sf = Simfile(simfile_contents);
	if (args.size() > 0) {
		CStringView checker = args.next();
		if (access(checker, F_OK) == 0 or checker.empty()) {
			replace_var_in_simfile(sf, "Simfile", simfile_contents, "checker", checker);
			stdlog("checker = ", checker);
		} else {
			errlog("Error: file: ", checker, " does not exist");
			return 1;
		}

	} else {
		sf.loadChecker();
		stdlog("checker = ", sf.checker);
	}

	return 0;
}

int statement(ArgvParser args) {
	STACK_UNWINDING_MARK;

	if (access("Simfile", F_OK) != 0) {
		errlog("Error: Simfile is missing. If you are sure that you are in the"
			" correct directory, running: `sip init . <name>` may help.");
		return 1;
	}

	auto simfile_contents = getFileContents("Simfile");
	Simfile sf = Simfile(simfile_contents);
	if (args.size() > 0) {
		CStringView statement = args.next();
		if (access(statement, F_OK) == 0 or statement.empty()) {
			replace_var_in_simfile(sf, "Simfile", simfile_contents, "statement", statement);
			stdlog("statement = ", statement);
		} else {
			errlog("Error: file: ", statement, " does not exist");
			return 1;
		}

	} else {
		sf.loadStatement();
		stdlog("statement = ", sf.statement);
	}	return 0;
}

int main_sol(ArgvParser args) {
	STACK_UNWINDING_MARK;

	if (access("Simfile", F_OK) != 0) {
		errlog("Error: Simfile is missing. If you are sure that you are in the"
			" correct directory, running: `sip init . <name>` may help.");
		return 1;
	}


	auto simfile_contents = getFileContents("Simfile");
	Simfile sf = Simfile(simfile_contents);
	if (args.size() > 0) {
		CStringView new_main_sol = args.next();
		if (access(new_main_sol, F_OK)) {
			errlog("Error: file: ", new_main_sol, " does not exist");
			return 1;
		}

		auto solutions = sf.configFile().getVar("solutions");
		if (not solutions.isSet()) {
			replace_var_in_simfile(sf, "Simfile", simfile_contents, "solutions", vector<string>{new_main_sol.to_string()});
		} else {
			auto sols = solutions.asArray();
			auto it = std::find(sols.begin(), sols.end(), new_main_sol);
			if (it != sols.end())
				sols.erase(it);

			sols.insert(sols.begin(), new_main_sol.to_string());
			replace_var_in_simfile(sf, "Simfile", simfile_contents, "solutions", sols);
		}

		stdlog("main solution = ", new_main_sol);

	} else {
		sf.loadSolutions();
		if (sf.solutions.empty())
			stdlog("no solution is set");
		else
			stdlog("main solution = ", sf.solutions.front());
	}

	return 0;
}

int mem(ArgvParser args) {
	STACK_UNWINDING_MARK;

	if (access("Simfile", F_OK) != 0) {
		errlog("Error: Simfile is missing. If you are sure that you are in the"
			" correct directory, running: `sip init . <name>` may help.");
		return 1;
	}

	auto simfile_contents = getFileContents("Simfile");
	Simfile sf = Simfile(simfile_contents);
	if (args.size() > 0) {
		if (not isDigitNotLessThan<1>(args.next())) {
			errlog("Error: memory limit is not a positive integer");
			return 1;
		}

		replace_var_in_simfile(sf, "Simfile", simfile_contents, "memory_limit", args.next());
		stdlog("mem = ", args.next());
	} else {
		sf.loadGlobalMemoryLimitOnly();
		stdlog("mem = ", sf.global_mem_limit >> 20);
	}

	return 0;
}

} // namespace command
