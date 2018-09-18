#include "compilation_cache.h"

using std::string;

namespace {
constexpr uint COMPILATION_ERRORS_MAX_LENGTH = 16 << 10; // 32 KB
constexpr timespec COMPILATION_TIME_LIMIT = {30, 0}; // 30 s
constexpr const char PROOT_PATH[] = "/usr/local/bin/proot";
} // anonymous namespace

decltype(concat()) CompilationCache::compile(StringView source) {
	STACK_UNWINDING_MARK;

	if (is_cached(source))
		return cached_path(source);

	sim::JudgeWorker jworker;
	string compilation_errors;
	auto tmplog = stdlog("Compiling ", source);
	tmplog.flush_no_nl();

	if (jworker.compileSolution(concat(source).to_cstr(),
		sim::filename_to_lang(source), COMPILATION_TIME_LIMIT,
		&compilation_errors, COMPILATION_ERRORS_MAX_LENGTH, PROOT_PATH))
	{
		tmplog(" failed.");
		errlog("Error: ", source, " compilation failed");
		errlog(compilation_errors);
		exit(1);
	}

	tmplog(" done.");
	auto cached_exec = cached_path(source);
	create_subdirectories(cached_exec);
	jworker.saveCompiledSolution(cached_exec.to_cstr());
	return cached_exec;
}

void CompilationCache::load_checker(sim::JudgeWorker& jworker, StringView checker) {
	STACK_UNWINDING_MARK;
	jworker.loadCompiledChecker(compile(checker).to_cstr());
}

void CompilationCache::load_solution(sim::JudgeWorker& jworker, StringView solution) {
	STACK_UNWINDING_MARK;
	jworker.loadCompiledSolution(compile(solution).to_cstr());
}
