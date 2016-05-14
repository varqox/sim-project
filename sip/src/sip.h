#pragma once

#include "simlib/include/logger.h"
#include "simlib/include/sim/simfile.h"

extern unsigned VERBOSITY; // 0 - quiet, 1 - normal, 2 or more - verbose
extern sim::Simfile pconf;

template<class... T>
void verbose_log(unsigned min_verbosity, const T&... args) {
	if (VERBOSITY >= min_verbosity)
		stdlog(args...);
}
