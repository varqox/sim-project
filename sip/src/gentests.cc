#include "simlib/include/debug.h"
#include "simlib/include/filesystem.h"
#include "simlib/include/process.h"
#include "simlib/include/string.h"

#include <cerrno>
#include <cstdlib>

#define foreach(i,x) for (__typeof(x.begin()) i = x.begin(), \
	i ##__end = x.end(); i != i ##__end; ++i)

using namespace std;

int main(int argc, char *argv[]) {
	if (argc < 2) {
		printf("Usage: %s <file with generating rules>\n",
			argv[0]);
		return 1;
	}

	vector<string> file = getFileByLines(argv[1], GFBL_IGNORE_NEW_LINES);
	foreach (line, file) {
		if (line.empty())
			continue;
		// Run somehow gen-line
	}
	return 0;
}
