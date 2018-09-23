#pragma once

#include <simlib/sim/judge_worker.h>

class CompilationCache {
	static auto cached_path(StringView path) {
		return concat("utils/cache/", path);
	}

public:
	static bool is_cached(StringView path) {
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

	static void clear() {
		if (remove_r("utils/cache/") and errno != ENOENT)
			THROW("remove_r()", errmsg());
	}

	static decltype(concat()) compile(StringView source);

	static void load_checker(sim::JudgeWorker& jworker, StringView checker);

	static void load_solution(sim::JudgeWorker& jworker, StringView solution);
};
