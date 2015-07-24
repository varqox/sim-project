#include "../include/debug.h"
#include "../include/filesystem.h"
#include "../include/process.h"

#include <cerrno>
#include <cstring>

using std::string;
using std::vector;

int compile(const string& source, const string& exec, unsigned verbosity,
		string* c_errors, size_t c_errors_max_len, const string& proot_path) {
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

	TemporaryDirectory tmp_dir("/tmp/tmp_dirXXXXXX");
	if (copy(source, tmp_dir.sname() + "a.cpp") == -1) {
		if (verbosity > 0)
			eprintf("Failed to copy source file - %s\n", strerror(errno));

		return 1;
	}

	if (verbosity > 1)
		printf("Compiling: '%s' ", (source).c_str());

	/* Compile as a 32-bit executable (not essential, but if the checker is
	*  x86_64 and Conver/Judge_machine is i386, then the checker will not work -
	*  this method is more secure (see making i386 syscall from x86_64))
	*  proot compiler to make compilation safer (e.g. prevent from including
	*  unwanted files)
	*/
	const char* args[] = {
		proot_path.c_str(),
		"-v", "-1",
		"-r", tmp_dir.name(),
		"-b", "/usr",
		"-b", "/bin",
		"-b", "/lib",
		"-b", "/lib32",
		"-b", "/libx32",
		"-b", "/lib64",
		"g++", // Invoke compiler
		"a.cpp",
		"-o", "exec",
		"-O2",
		"-static",
		"-lm",
		"-m32",
		NULL
	};

	// TODO: add compilation time limit
	// Run compiler
	spawn_opts sopts = { -1, -1, cef };
	int compile_status = spawn(args[0], args, &sopts);

	// Check for errors
	if (compile_status != 0) {
		if (verbosity > 1)
			printf("Failed with code: %i.\n", compile_status);

		if (c_errors)
			*c_errors = getFileContents(cef, 0, c_errors_max_len);

		// Clean up
		if (cef >= 0)
			while (close(cef) == -1 && errno == EINTR) {}
		return 2;
	}

	if (verbosity > 1)
		printf("Completed successfully.\n");

	if (c_errors)
		*c_errors = "";

	// Clean up
	if (cef >= 0)
		while (close(cef) == -1 && errno == EINTR) {}

	// Move exec
	if (move(tmp_dir.sname() + "exec", exec) == -1) {
		if (verbosity > 0)
			eprintf("Failed to move exec - %s\n", strerror(errno));

		return 1;
	}

	return 0;
}
