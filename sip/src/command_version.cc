#include "git_commit.hh"

#include <cstdio>

namespace commands {

void version() noexcept {
	printf("Version: %s\nBuilt on %s at %s\n", COMMIT, __DATE__, __TIME__);
}

} // namespace commands
