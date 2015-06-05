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

int compile(const string& source, const string& exec, string* c_errors,
		size_t c_errors_max_len) {
	int cef = -1;
	if (c_errors) {
		cef = getUnlinkedTmpFile();
		if (cef == -1) {
			E("Failed to open 'compile_errors' - %s\n", strerror(errno));
			return 1;
		}
	}

	E("Compiling: '%s' ", (source).c_str());

	// Run compiler
	vector<string> args;
	append(args)("g++")("-O2")("-static")("-lm")(source)("-o")(exec);

	/* Compile as 32 bit executable (not essential but if checker will be x86_63
	*  and Conver/Judge_machine i386 then checker won't work, with it its more
	*  secure (see making i386 syscall from x86_64))
	*/
	append(args)("-m32");

#ifdef DEBUG
	E("(Command: '%s", args[0].c_str());
	for (size_t i = 1, end = args.size(); i < end; ++i)
		E(" %s", args[i].c_str());
	E("')\n");
#endif

	spawn_opts sopts = { -1, -1, cef };
	int compile_status = spawn(args[0], args, &sopts);

	// Check for errors
	if (compile_status != 0) {
		E("Failed.\n");

		if (c_errors)
			*c_errors = getFileContents(cef, 0, c_errors_max_len);

		if (cef >= 0)
			while (close(cef) == -1 && errno == EINTR) {}
		return 2;

	}

	E("Completed successfully.\n");

	if (c_errors)
		*c_errors = "";

	if (cef >= 0)
		while (close(cef) == -1 && errno == EINTR) {}
	return 0;
}
