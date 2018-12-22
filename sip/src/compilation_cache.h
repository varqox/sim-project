#pragma once

#include "sip_package.h"

#include <simlib/sim/judge_worker.h>

class SipPackage::CompilationCache {
	static auto cached_path(StringView path) {
		return concat("utils/cache/", path);
	}

public:
	static bool is_cached(StringView path);

	static void clear();

	static decltype(concat()) compile(StringView source);

	static void load_checker(sim::JudgeWorker& jworker, StringView checker);

	static void load_solution(sim::JudgeWorker& jworker, StringView solution);
};
