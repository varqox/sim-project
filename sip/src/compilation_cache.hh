#pragma once

#include "simlib/sim/judge_worker.hh"
#include "src/sip_package.hh"

class SipPackage::CompilationCache {
    static auto cached_path(StringView path) { return concat("utils/cache/", path); }

public:
    static bool is_cached(StringView path);

    static void clear();

    static decltype(concat()) compile(StringView source);

    static void load_checker(sim::JudgeWorker& jworker);

    static void load_solution(sim::JudgeWorker& jworker, StringView solution);
};
