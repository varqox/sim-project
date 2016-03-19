#include "../include/filesystem.h"
#include "../include/logger.h"
#include "../include/process.h"

using std::string;
using std::vector;

int compile(const string& source, const string& exec, unsigned verbosity,
	uint64_t time_limit, string* c_errors, size_t c_errors_max_len,
	const string& proot_path)
{
	int cef = -1;
	if (c_errors) {
		cef = getUnlinkedTmpFile();
		if (cef == -1)
			throw std::runtime_error(concat("Failed to open 'compile_errors'",
				error(errno)));
	}
	Closer closer(cef);

	TemporaryDirectory tmp_dir("/tmp/tmp_dirXXXXXX");
	if (BLOCK_SIGNALS(copy(source, concat(tmp_dir.name(), "a.cpp"))) == -1)
		throw std::runtime_error(concat("Failed to copy source file",
			error(errno)));

	if (verbosity > 1)
		stdlog("Compiling: '", source, "' ");

	/* Compile as a 32-bit executable (not essential, but if the checker is
	*  x86_64 and Conver/Judge_machine is i386, then the checker will not work -
	*  this method is more secure (see making i386 syscall from x86_64))
	*  proot compiler to make compilation safer (e.g. prevent from including
	*  unwanted files)
	*/
	vector<string> args = std::initializer_list<string> {
		proot_path,
		"-v", "-1",
		"-r", tmp_dir.sname(),
		"-b", "/usr",
		"-b", "/bin",
		"-b", "/lib",
		"-b", "/lib32",
		"-b", "/libx32",
		"-b", "/lib64",
		"-b", "/etc/alternatives/",
		"g++", // Invoke compiler
		"a.cpp",
		"-o", "exec",
		"-O2",
		"-std=c++11",
		"-static",
		"-lm",
		"-m32"
	};

	// Run compiler
	Spawner::ExitStat es = Spawner::run(args[0], args,
		{-1, cef, cef, time_limit, 1 << 30 /* 1 GiB */});

	// Check for errors
	if (es.code != 0) {
		if (verbosity > 1)
			stdlog(es.message);

		if (c_errors)
			*c_errors = (es.runtime >= time_limit
				? "Compilation time limit exceeded"
				: getFileContents(cef, 0, c_errors_max_len));

		return 2;
	}

	if (verbosity > 1)
		stdlog("Completed successfully.");

	if (c_errors)
		*c_errors = "";

	// Move exec
	if (BLOCK_SIGNALS(move(concat(tmp_dir.name(), "exec"), exec, false)) == -1)
		throw std::runtime_error(concat("Failed to move exec", error(errno)));

	return 0;
}
