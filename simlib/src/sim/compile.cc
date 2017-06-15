#include "../../include/spawner.h"

using std::string;
using std::vector;

namespace sim {

int compile(CStringView source, CStringView exec, unsigned verbosity,
	timespec time_limit, string* c_errors, size_t c_errors_max_len,
	const std::string& proot_path)
{
	FileDescriptor cef;
	if (c_errors) {
		cef = openUnlinkedTmpFile();
		if (cef == -1)
			THROW("Failed to open 'compile_errors'", error(errno));
	}

	TemporaryDirectory tmp_dir("/tmp/tmp_dirXXXXXX");
	if (copy(source, concat_tostr(tmp_dir.name(), "a.cpp")))
		THROW("Failed to copy source file", error(errno));

	if (verbosity > 1)
		stdlog("Compiling: `", source, '`');

	/* Compile as a 32-bit executable (not essential, but if the checker is
	*  x86_64 and tracer is i386, then the checker will not work -
	*  this method is more secure (see making i386 syscall from x86_64)).
	*  Compiler is prooted to make compilation safer (e.g. prevents including
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
		// Invoke the compiler
		"g++",
		"a.cpp",
		"-o", "exec",
		"-O3", // These days -O3 option is safe to use
		"-std=c++11",
		"-static",
		"-lm",
		"-m32"
	};

	// Run compiler
	Spawner::ExitStat es = Spawner::run(args[0], args,
		{-1, cef, cef, time_limit, 1 << 30 /* 1 GiB */});

	// Check for errors
	if (es.si.code != CLD_EXITED or es.si.status != 0) {
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
	if (move(concat_tostr(tmp_dir.name(), "exec"), exec, false))
		THROW("Failed to move exec", error(errno));

	return 0;
}

} // namespace sim
