#include "compilation_cache.hh"
#include "constants.hh"
#include "sip_error.hh"

#include <chrono>
#include <simlib/concat_tostr.hh>
#include <simlib/file_info.hh>
#include <simlib/file_perms.hh>
#include <simlib/sim/judge/disk_compilation_cache.hh>
#include <unistd.h>

using std::string;

namespace compilation_cache {

sim::judge::DiskCompilationCache get_cache() {
    (void)mkdir("utils/", S_0755);
    return sim::judge::DiskCompilationCache{"utils/cache/", std::chrono::hours{7 * 24}};
}

void clear() {
    STACK_UNWINDING_MARK;

    if (remove_r("utils/cache/") and errno != ENOENT) {
        THROW("remove_r()", errmsg());
    }
}

} // namespace compilation_cache
