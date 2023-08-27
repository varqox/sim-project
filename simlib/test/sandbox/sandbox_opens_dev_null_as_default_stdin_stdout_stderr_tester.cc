#include <cstddef>
#include <fcntl.h>
#include <simlib/throw_assert.hh>
#include <sys/stat.h>
#include <unistd.h>

int main() {
    struct stat st;
    throw_assert(stat("/dev/null", &st) == 0);
    throw_assert(S_ISCHR(st.st_mode));
    dev_t null_dev_num = st.st_rdev;
    // Check if stdin, stdout and stderr are all /dev/null
    for (int fd : {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO}) {
        throw_assert(fstat(fd, &st) == 0);
        throw_assert(S_ISCHR(st.st_mode));
        throw_assert(st.st_rdev == null_dev_num);
    }
    // Check readability and writeability
    int rc = fcntl(STDIN_FILENO, F_GETFL);
    throw_assert(rc != -1 && (rc & O_ACCMODE) == O_RDONLY);
    rc = fcntl(STDOUT_FILENO, F_GETFL);
    throw_assert(rc != -1 && (rc & O_ACCMODE) == O_WRONLY);
    rc = fcntl(STDERR_FILENO, F_GETFL);
    throw_assert(rc != -1 && (rc & O_ACCMODE) == O_WRONLY);
    return 0;
}
