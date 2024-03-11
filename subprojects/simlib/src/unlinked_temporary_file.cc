#include <cerrno>
#include <cstdlib>
#include <simlib/unlinked_temporary_file.hh>
#include <sys/stat.h>
#include <simlib/file_descriptor.hh>
#include <unistd.h>

FileDescriptor open_unlinked_tmp_file(int flags) noexcept {
    FileDescriptor fd;
    fd = open("/tmp", O_TMPFILE | O_RDWR | O_EXCL | flags, S_0600);
    if (fd != -1) {
        return fd;
    }

    if (errno != EOPNOTSUPP && errno != EINVAL) {
        return fd;
    }

    char name[] = "/tmp/tmp_unlinked_file.XXXXXX";
    umask(077); // Only owner can access this temporary file
    fd = mkostemp(name, flags);
    if (fd == -1) {
        return fd;
    }

    (void)unlink(name);
    return fd;
}
