#pragma once

#include "../include/filesystem.h"
#include "../include/memory.h"

extern UniquePtr<TemporaryDirectory> tmp_dir;
extern unsigned VERBOSITY; // 0 - quiet, 1 - normal, 2 or more - verbose
