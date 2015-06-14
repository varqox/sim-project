#pragma once

#include "../include/filesystem.h"
#include "../include/memory.h"
#include "../include/sim_problem.h"

extern UniquePtr<TemporaryDirectory> tmp_dir;
extern bool GEN_OUT, VALIDATE_OUT, USE_CONF, FORCE_AUTO_LIMIT;
extern unsigned VERBOSITY; // 0 - quiet, 1 - normal, 2 or more - verbose
extern unsigned long long MEMORY_LIMIT; // in bytes
extern unsigned long long HARD_TIME_LIMIT, TIME_LIMIT; // in usec
extern UniquePtr<directory_tree::node> package_tree_root;
extern ProblemConfig conf_cfg;
