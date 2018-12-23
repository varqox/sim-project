#include "compilation_cache.h"
#include "proot_dump.h"
#include "sip_error.h"

using std::string;

namespace {
constexpr uint COMPILATION_ERRORS_MAX_LENGTH = 16 << 10; // 32 KB
constexpr timespec COMPILATION_TIME_LIMIT = {30, 0}; // 30 s
constexpr const char PROOT_PATH[] = "proot";
} // anonymous namespace

bool SipPackage::CompilationCache::is_cached(StringView path) {
	STACK_UNWINDING_MARK;

	struct stat64 path_stat, cached_stat;
	if (stat64(FilePath(cached_path(path)), &cached_stat)) {
		if (errno == ENOENT)
			return false; // File does not exist

		THROW("stat64()", errmsg());
	}

	if (stat64(FilePath(concat(path)), &path_stat))
		THROW("stat64()", errmsg());

	// The cached file cannot be older than the source file
	return (path_stat.st_mtime <= cached_stat.st_mtime);
}

void SipPackage::CompilationCache::clear() {
	STACK_UNWINDING_MARK;

	if (remove_r("utils/cache/") and errno != ENOENT)
		THROW("remove_r()", errmsg());
}

void cache_proot(FilePath dest_file) {
	STACK_UNWINDING_MARK;

	if (access(dest_file, F_OK) == 0)
		return;

	create_subdirectories(dest_file.to_cstr());
	putFileContents(dest_file, (const char*)proot_dump, proot_dump_len, S_0700);
}

decltype(concat()) SipPackage::CompilationCache::compile(StringView source) {
	STACK_UNWINDING_MARK;

	if (is_cached(source))
		return cached_path(source);

	cache_proot(cached_path(PROOT_PATH));

	sim::JudgeWorker jworker;
	string compilation_errors;
	auto tmplog = stdlog("Compiling ", source);
	tmplog.flush_no_nl();

	if (jworker.compileSolution(concat(source),
		sim::filename_to_lang(source), COMPILATION_TIME_LIMIT,
		&compilation_errors, COMPILATION_ERRORS_MAX_LENGTH,
		cached_path(PROOT_PATH).to_string()))
	{
		tmplog(" failed.");
		errlog(compilation_errors);
		throw SipError("Error: ", source, " compilation failed");
	}

	tmplog(" done.");
	auto cached_exec = cached_path(source);
	create_subdirectories(cached_exec);
	jworker.saveCompiledSolution(cached_exec);
	return cached_exec;
}

void SipPackage::CompilationCache::load_checker(sim::JudgeWorker& jworker, StringView checker) {
	STACK_UNWINDING_MARK;
	jworker.loadCompiledChecker(compile(checker));
}

void SipPackage::CompilationCache::load_solution(sim::JudgeWorker& jworker, StringView solution) {
	STACK_UNWINDING_MARK;
	jworker.loadCompiledSolution(compile(solution));
}
