#include "simlib/unlinked_temporary_file.hh"

#include <sys/stat.h>

FileDescriptor open_unlinked_tmp_file(int flags) noexcept {
    FileDescriptor fd;
#ifdef O_TMPFILE
    fd = open("/tmp", O_TMPFILE | O_RDWR | O_EXCL | flags, S_0600);
    if (fd != -1) {
        return fd;
    }

    // If errno == EINVAL, then fall back to mkostemp(3)
    if (errno != EINVAL) {
        return fd;
    }
#endif

    char name[] = "/tmp/tmp_unlinked_file.XXXXXX";
    umask(077); // Only owner can access this temporary file
    fd = mkostemp(name, flags);
    if (fd == -1) {
        return fd;
    }

    (void)unlink(name);
    return fd;
}
