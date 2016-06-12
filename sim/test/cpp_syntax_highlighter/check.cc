#include <dirent.h>
#include <sim/cpp_syntax_highlighter.h>
#include <simlib/process.h>
#include <simlib/utilities.h>

using namespace std;

vector<string> findTests(string path = "tests") {
	if (path.empty())
		return {};

	DIR *dir = opendir(path.data());
	if (dir == nullptr)
		THROW("opendir()", error(errno));

	auto close_dir = [&]{ closedir(dir); };
	CallInDtor<decltype(close_dir)> dir_guard(close_dir);

	// Collect .in and .out files
	vector<string> in, out;

	dirent *file;
	while ((file = readdir(dir)))
		if (isSuffix(file->d_name, ".in")) {
			in.emplace_back(file->d_name, strlen(file->d_name) - 3);
		} else if (isSuffix(file->d_name, ".out")) {
			out.emplace_back(file->d_name, strlen(file->d_name) - 4);
		}

	sort(in);

	// For each .out check if is paired with any .in and if it is, collect it
	vector<string> res;
	for (auto&& str : out)
		if (binary_search(in, str))
			res.emplace_back(str);

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
			if (show_diff)
				system(concat("git --no-pager diff --no-index '", out_fname,
					"' '", ans_fname, '\'').data());
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

// argv[1] == "diff" -> show diff if test failed
// argv[1] == "regen" -> regenerate .out files
int main(int argc, char **argv) {
	chdirToExecDir();

	stdlog.label(false);
	errlog.label(false);

	bool diff = false, regen = false;
	if (argc > 1) {
		if (!strcmp(argv[1], "diff"))
			diff = true;
		else if (!strcmp(argv[1], "regen"))
			regen = true;
	}

	vector<string> tests = findTests();

	if (regen) {
		regenerate(tests);
		return 0;
	}

	if (check(tests, diff) != tests.size())
		return 1;

	return 0;
}
