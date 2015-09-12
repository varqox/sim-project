#pragma once

#include "../simlib/include/filesystem.h"
#include "../simlib/include/memory.h"

extern UniquePtr<TemporaryDirectory> tmp_dir;
extern unsigned VERBOSITY; // 0 - quiet, 1 - normal, 2 or more - verbose