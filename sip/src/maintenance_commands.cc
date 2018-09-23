#include "commands.h"
#include "compilation_cache.h"
#include "sip.h"

#include <simlib/process.h>

namespace command {

int package(ArgvParser args) {
	STACK_UNWINDING_MARK;

	if (access("Simfile", F_OK) != 0) {
		errlog("Error: Simfile is missing. If you are sure that you are in the"
			" correct directory, running: `sip init . <name>` may help.");
		return 1;
	}

	THROW("Unipmlemented");
	return 0;
}

int prepare(ArgvParser args) {
	STACK_UNWINDING_MARK;

	if (access("Simfile", F_OK) != 0) {
		errlog("Error: Simfile is missing. If you are sure that you are in the"
			" correct directory, running: `sip init . <name>` may help.");
		return 1;
	}

	THROW("Unipmlemented");
	return 0;
}

int clean(ArgvParser args) {
	STACK_UNWINDING_MARK;

	if (access("Simfile", F_OK) != 0) {
		errlog("Error: Simfile is missing. If you are sure that you are in the"
			" correct directory, running: `sip init . <name>` may help.");
		return 1;
	}

	CompilationCache::clear();
	if (remove_r("utils/latex/") and errno != ENOENT)
		THROW("remove_r()", errmsg());

	while (args.size() > 0) {
		auto arg = args.extract_next();
		if (arg == "tests") {
			// TODO: remove generated tests
			THROW("Unimplemented");

		} else
			stdlog("\033[1;35mwarning\033[m: unrecognized argument: ", arg);
	}

	return 0;
}

int zip(ArgvParser args) {
	clean(args);
	auto cwd = getCWD();
	cwd.size--;
	CStringView dir = filename(cwd.to_cstr());

	// Create zip
	{
		DirectoryChanger dc("..");
		compress_into_zip({dir.data()}, concat(dir, ".zip"));
	}

	return 0;
}

} // namespace command
