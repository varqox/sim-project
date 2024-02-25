#pragma once

#include "sip_package.hh"

#include <simlib/sim/judge/disk_compilation_cache.hh>
#include <simlib/sim/judge_worker.hh>

namespace compilation_cache {

sim::judge::DiskCompilationCache get_cache();

void clear();

} // namespace compilation_cache
