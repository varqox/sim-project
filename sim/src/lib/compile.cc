#include "../include/compile.h"
#include "../include/debug.h"
#include "../include/filesystem.h"
#include "../include/process.h"
#include "../include/utility.h"

#include <cerrno>
#include <vector>
#include <cstring>

using std::string;
using std::vector;

int compile(const string& source, const string& exec, unsigned verbosity,
		string* c_errors, size_t c_errors_max_len) {
	int cef = -1;
	if (c_errors) {
		cef = getUnlinkedTmpFile();
		if (cef == -1) {
			if (verbosity > 0)
				eprintf("Failed to open 'compile_errors' - %s\n",
					strerror(errno));

			return 1;
		}
	}

	if (verbosity > 1)
		printf("Compiling: '%s' ", (source).c_str());

	// Run compiler
	vector<string> args;
	append(args)("g++")("-O2")("-static")("-lm")(source)("-o")(exec);

	/* Compile as 32 bit executable (not essential but if checker will be x86_63
	*  and Conver/Judge_machine i386 then checker won't work, with it its more
	*  secure (see making i386 syscall from x86_64))
	*/
	append(args)("-m32");

	if (verbosity > 1) {
		printf("(Command: '%s", args[0].c_str());
		for (size_t i = 1, end = args.size(); i < end; ++i)
			printf(" %s", args[i].c_str());
		printf("')\n");
	}

	spawn_opts sopts = { -1, -1, cef };
	int compile_status = spawn(args[0], args, &sopts);

	// Check for errors
	if (compile_status != 0) {
		if (verbosity > 1)
			printf("Failed.\n");

		if (c_errors)
			*c_errors = getFileContents(cef, 0, c_errors_max_len);

		if (cef >= 0)
			while (close(cef) == -1 && errno == EINTR) {}
		return 2;

	}

	if (verbosity > 1)
		printf("Completed successfully.\n");

	if (c_errors)
		*c_errors = "";

	if (cef >= 0)
		while (close(cef) == -1 && errno == EINTR) {}
	return 0;
}
