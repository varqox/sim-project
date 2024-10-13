#include <simlib/am_i_root.hh>
#include <simlib/errmsg.hh>
#include <simlib/macros/throw.hh>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

AmIRoot am_i_root() {
    struct stat st;
    if (stat("/dev/null", &st)) {
        THROW("stat()", errmsg());
    }
    bool dev_null_is_dev_null = S_ISCHR(st.st_mode) && st.st_rdev == makedev(1, 3);
    auto root_real_uid = st.st_uid;
    auto root_real_gid = st.st_gid;

    auto euid = geteuid();
    auto egid = getegid();

    if (euid == 0 || egid == 0) {
        return AmIRoot::YES;
    }
    if (!dev_null_is_dev_null) {
        return AmIRoot::MAYBE;
    }
    if (euid == root_real_uid || egid == root_real_gid) {
        return AmIRoot::YES;
    }
    return AmIRoot::NO;
}
