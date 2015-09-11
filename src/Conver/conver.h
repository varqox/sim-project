#pragma once

#include "../simlib/include/filesystem.h"
#include "../simlib/include/memory.h"
#include "../simlib/include/sim_problem.h"

extern UniquePtr<TemporaryDirectory> tmp_dir;
extern bool GEN_OUT, VALIDATE_OUT, USE_CONFIG, FORCE_AUTO_LIMIT;
extern unsigned VERBOSITY; // 0 - quiet, 1 - normal, 2 or more - verbose
extern unsigned long long MEMORY_LIMIT; // in bytes
extern unsigned long long HARD_TIME_LIMIT, TIME_LIMIT; // in usec
extern std::string PROOT_PATH;
extern UniquePtr<directory_tree::node> package_tree_root;
extern ProblemConfig config_conf;