#include "simlib/include/debug.h"
#include "simlib/include/filesystem.h"
#include "simlib/include/process.h"
#include "simlib/include/string.h"

#include <cerrno>
#include <cstdlib>

#define foreach(i,x) for (__typeof(x.begin()) i = x.begin(), \
	i ##__end = x.end(); i != i ##__end; ++i)

using namespace std;

typedef unsigned long long ULL;

void generate(string test_name, const vector<string>& args) {
	test_name += ".in";
	FILE *test = fopen((test_name).c_str(), "w");
	if (test == NULL) {
		eprintf("Failed to open: '%s' - %s\n", test_name.c_str(),
			strerror(errno));
		exit(-1);
	}

	Spawner::ExitStat es = Spawner::run(args[0], args, {-1, fileno(test), STDERR_FILENO});
	if (es.code) {
		eprintf("Generator returned non-zero code\n");
		exit(-1);
	}

	fclose(test);
}

struct Test {
	unsigned group_id;
	string test_id;
};

int main(int argc, char *argv[]) {
	if (argc < 3) {
		printf("Usage: %s <test specifier> <generator> [generator args...]\n",
			argv[0]);
		return 1;
	}

	string test_prefix = "in/";
	// remove_r(test_prefix.c_str());
	if (mkdir(test_prefix) && errno != EEXIST) {
		eprintf("Error: mkdir('%s') - %s\n", test_prefix.c_str(),
			strerror(errno));
		return 2;
	}

	Test begin, end;
	// test_prefix
	size_t pos = 0, tmp;
	while (isalpha(argv[1][pos]))
		++pos;
	test_prefix += string(argv[1], argv[1] + pos);

	// begin
	// group_id
	tmp = pos;
	while (isdigit(argv[1][pos]))
		++pos;
	if (strtou(argv[1], &begin.group_id, tmp, pos) < 1)
		throw std::runtime_error("Wrong test group id");
	// test_id
	tmp = pos;
	while (isalpha(argv[1][pos]))
		++pos;
	begin.test_id.assign(argv[1] + tmp, argv[1] + pos);
	if (begin.test_id.size() > 1)
		throw std::runtime_error("Too long test test id");

	// end
	if (argv[1][pos] == '-') {
		// group_id
		tmp = ++pos;
		while (isdigit(argv[1][pos]))
			++pos;
		if (tmp == pos)
			end.group_id = begin.group_id;
		else if (strtou(argv[1], &end.group_id, tmp, pos) < 1)
			throw std::runtime_error("Wrong test group id");
		// test_id
		tmp = pos;
		while (isalpha(argv[1][pos]))
			++pos;
		end.test_id.assign(argv[1] + tmp, argv[1] + pos);
		if (end.test_id.size() > 1)
			throw std::runtime_error("Too long test test id");

	} else
		end = begin;

	// Log
	E("begin: (%u, %s)\n", begin.group_id, begin.test_id.c_str());
	E("end: (%u, %s)\n", end.group_id, end.test_id.c_str());

	// More validation
	if (begin.test_id.empty() != end.test_id.empty())
		throw std::runtime_error("Only none or both test can have test id");

	if (begin.group_id > end.group_id)
		throw std::runtime_error("Begin test group id cannot be grater than "
			"end test group id");

	// Generator args
	vector<string> args(argv + 2, argv + argc);
	D(
		foreach (i, args)
			E(i == args.begin() ? "%s" : ", %s", i->c_str());
		E("\n");
	)

	// Generating
	for (unsigned group_id = begin.group_id; group_id <= end.group_id;
			++group_id) {
		if (begin.test_id.empty()) {
			printf("%u: ", group_id);
			fflush(stdout);

			generate(test_prefix + toString((ULL)group_id), args);

			printf("generated.\n");

		} else
			for (char test_id = begin.test_id[0]; test_id <= end.test_id[0];
					++test_id) {
				printf("%u%c: ", group_id, test_id);
				fflush(stdout);

				generate(test_prefix + toString((ULL)group_id) + test_id, args);

				printf("generated.\n");
			}

		printf("\n");
	}
	return 0;
}
