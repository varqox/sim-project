#pragma once

#include <simlib/sim/judge_worker.h>

class CompilationCache {
	static auto cached_path(StringView path) {
		return concat("utils/cache/", path);
	}

public:
	static bool is_cached(StringView path) {
		return (access(cached_path(path).to_cstr(), F_OK) == 0);
	}

	static void clear() {
		if (remove_r("utils/cache/") and errno != ENOENT)
			THROW("remove_r()", errmsg());
	}

	static decltype(concat()) compile(StringView source);

	static void load_checker(sim::JudgeWorker& jworker, StringView checker);

	static void load_solution(sim::JudgeWorker& jworker, StringView solution);
};
