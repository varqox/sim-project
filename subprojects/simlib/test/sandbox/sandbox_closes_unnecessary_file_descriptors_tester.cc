#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <simlib/throw_assert.hh>

int main() {
    DIR* dir = opendir("/proc/self/fd");
    throw_assert(dir != nullptr);
    int dir_fd = dirfd(dir);
    throw_assert(dir_fd > 2 && "standard file descriptors should be set");
    char dirfd_str[32];
    throw_assert(snprintf(dirfd_str, sizeof(dirfd_str), "%i", dir_fd) > 0);

    for (;;) {
        errno = 0;
        struct dirent* file = readdir(dir); // NOLINT(concurrency-mt-unsafe)
        if (file == nullptr) {
            throw_assert(errno == 0);
            break;
        }

        const char* allowed_filenames[] = {".", "..", "0", "1", "2", dirfd_str};
        for (auto allowed_filename : allowed_filenames) {
            if (strcmp(file->d_name, allowed_filename) == 0) {
                goto entry_ok;
            }
        }

        (void)fprintf(stderr, "unexpected entry: %s\n", file->d_name);
        throw_assert(false && "unexpected entry");
    entry_ok:;
    }
    (void)closedir(dir);

    return 0;
}
