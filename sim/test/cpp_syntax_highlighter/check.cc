#include <cmath>
#include <dirent.h>
#include <sim/cpp_syntax_highlighter.h>
#include <simlib/process.h>
#include <simlib/spawner.h>
#include <simlib/utilities.h>

using std::vector;
using std::string;

vector<string> findTests(string path = "tests") {
	if (path.empty())
		return {};

	DIR *dir = opendir(path.data());
	if (dir == nullptr)
		THROW("opendir('", path, "')", error(errno));

	auto close_dir = [&]{ closedir(dir); };
	CallInDtor<decltype(close_dir)> dir_guard(close_dir);

	// Collect .in and .out files
	vector<string> in, out;

	dirent *file;
	while ((file = readdir(dir)))
		if (hasSuffix(file->d_name, ".in")) {
			in.emplace_back(file->d_name, strlen(file->d_name) - 3);
		} else if (hasSuffix(file->d_name, ".out")) {
			out.emplace_back(file->d_name, strlen(file->d_name) - 4);
		}

	sort(in);

	// For each .out check if is paired with any .in and if it is, collect it
	vector<string> res;
	for (auto&& str : out)
		if (binary_search(in, str))
			res.emplace_back(str);

	sort(res, StrNumCompare());
	return res;
}

uint check(const vector<string>& tests, bool show_diff = false) {
	CppSyntaxHighlighter csh;
	uint passed = 0;
	for (auto&& test : tests) {
		printf("%6s: ", test.c_str());
		string in_fname = concat("tests/", test, ".in");
		string out_fname = concat("tests/", test, ".out");
		string ans = csh(getFileContents(in_fname));

		// OK
		if (ans == getFileContents(out_fname)) {
			++passed;
			puts("\033[1;32mOK\033[m");

		// WRONG
		} else {
			puts("\033[1;31mWRONG\033[m");
			string ans_fname = concat("tests/", test, ".ans");
			putFileContents(ans_fname, ans);
			if (show_diff) {
				(void)Spawner::run("git", {
					"git",
					"--no-pager",
					"diff",
					"--no-index",
					"-a",
					"--color=always",
					out_fname,
					ans_fname
				}, {-1, STDOUT_FILENO, STDERR_FILENO});
			}
		}
	}

	printf("Passed %u / %u (%u%%)\n", passed, (uint)tests.size(),
		(uint)round(double(passed * 100)/tests.size()));
	return passed;
}

void regenerate(const vector<string>& tests) {
	CppSyntaxHighlighter csh;
	for (auto&& test : tests) {
		string in_fname = concat("tests/", test, ".in");
		string out_fname = concat("tests/", test, ".out");
		putFileContents(out_fname, csh(getFileContents(in_fname)));
	}
}

// argv[1] == "--diff" -> show diff if test failed
// argv[1] == "--regen" -> regenerate ALL .out files
int main(int argc, char **argv) {
	try {
		stdlog.label(false);
		errlog.label(false);

		bool diff = false, regen = false;
		vector<string> arg_tests;
		for (int i = 1; i < argc; ++i) {
			if (!strcmp(argv[i], "--diff"))
				diff = true;
			else if (!strcmp(argv[i], "--regen"))
				regen = true;
			else
				arg_tests.emplace_back(argv[i]);
		}

		chdirToExecDir();
		vector<string> tests = findTests();

		if (regen) {
			regenerate(tests);
			return 0;
		}

		// Take these arg_tests, which were detected and run check on them
		if (arg_tests.size()) {
			sort(tests);
			vector<string> res;
			for (auto&& str : arg_tests) {
				if (binary_search(tests, str))
					res.emplace_back(str);
				else
					stdlog("Ignored: '", str, "' - not detected");
			}
			tests = std::move(res);
		}

		if (check(tests, diff) != tests.size())
			return 2;


	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return 1;
	}

	return 0;
}
