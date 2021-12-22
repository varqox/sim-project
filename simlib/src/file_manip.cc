#include "simlib/file_manip.hh"
#include "simlib/call_in_destructor.hh"
#include "simlib/directory.hh"
#include "simlib/file_descriptor.hh"
#include "simlib/file_path.hh"
#include "simlib/inplace_buff.hh"
#include "simlib/proc_stat_file_contents.hh"
#include "simlib/random.hh"
#include "simlib/repeating.hh"
#include "simlib/string_transform.hh"
#include "simlib/syscalls.hh"
#include "simlib/temporary_file.hh"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <linux/limits.h>
#include <new>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

using std::array;
using std::string;

int mkdir_r(string path, mode_t mode) noexcept {
    if (path.size() >= PATH_MAX) {
        errno = ENAMETOOLONG;
        return -1;
    }

    // Add ending slash (if not exists)
    if (path.empty() || path.back() != '/') {
        path += '/';
    }

    size_t end = 1; // If there is a leading slash, it will be omitted
    while (end < path.size()) {
        while (path[end] != '/') {
            ++end;
        }

        path[end] = '\0'; // Separate subpath
        if (mkdir(path.data(), mode) == -1 && errno != EEXIST) {
            return -1;
        }

        path[end++] = '/';
    }

    return 0;
}

/**
 * @brief Removes recursively directory @p path relative to a directory
 *   file descriptor @p dirfd
 *
 * @param dirfd directory file descriptor
 * @param path directory pathname (relative to @p dirfd)
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for fstatat64(2), openat(2), unlinkat(2),
 *   fdopendir(3)
 */
static int remove_rat_impl(int dirfd, FilePath path) noexcept {
    int fd = openat(dirfd, path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd == -1) {
        return unlinkat(dirfd, path, 0);
    }

    DIR* dir = fdopendir(fd);
    if (dir == nullptr) {
        close(fd);
        return unlinkat(dirfd, path, AT_REMOVEDIR);
    }

    int ec = 0;
    int rc = 0;
    for_each_dir_component(
            dir,
            [&](dirent* file) -> repeating {
#ifdef _DIRENT_HAVE_D_TYPE
                if (file->d_type == DT_DIR || file->d_type == DT_UNKNOWN) {
#endif
                    if (remove_rat_impl(fd, file->d_name)) {
                        ec = errno;
                        rc = -1;
                        return stop_repeating;
                    }
#ifdef _DIRENT_HAVE_D_TYPE
                } else if (unlinkat(fd, file->d_name, 0)) {
                    ec = errno;
                    rc = -1;
                    return stop_repeating;
                }
#endif

                return continue_repeating;
            },
            [&] {
        ec = errno;
        rc = -1;
            });

    (void)closedir(dir);

    if (rc == -1) {
        errno = ec;
        return -1;
    }

    return unlinkat(dirfd, path, AT_REMOVEDIR);
}

int remove_rat(int dirfd, FilePath path) noexcept { return remove_rat_impl(dirfd, path); }

int remove_dir_contents_at(int dirfd, FilePath pathname) noexcept {
    int fd = openat(dirfd, pathname, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd == -1) {
        return -1;
    }

    DIR* dir = fdopendir(fd);
    if (dir == nullptr) {
        int ec = errno;
        close(fd);
        errno = ec;
        return -1;
    }

    int ec = 0;
    int rc = 0;
    for_each_dir_component(
            dir,
            [&](dirent* file) -> repeating {
#ifdef _DIRENT_HAVE_D_TYPE
                if (file->d_type == DT_DIR || file->d_type == DT_UNKNOWN) {
#endif
                    if (remove_rat_impl(fd, file->d_name)) {
                        ec = errno;
                        rc = -1;
                        return stop_repeating;
                    }
#ifdef _DIRENT_HAVE_D_TYPE
                } else if (unlinkat(fd, file->d_name, 0)) {
                    ec = errno;
                    rc = -1;
                    return stop_repeating;
                }
#endif

                return continue_repeating;
            },
            [&] {
        ec = errno;
        rc = -1;
            });

    (void)closedir(dir);

    if (rc == -1) {
        errno = ec;
    }

    return rc;
}

int create_subdirectories(FilePath file) noexcept {
    StringView path(file);
    path.remove_trailing([](char c) { return (c != '/'); });
    if (path.empty()) {
        return 0;
    }

    return mkdir_r(path.to_string());
}

int blast(int infd, int outfd) noexcept {
    array<char, 65536> buff{};
    ssize_t len = 0;
    ssize_t written = 0;
    while (len = read(infd, buff.data(), buff.size()),
            len > 0 || (len == -1 && errno == EINTR)) {
        ssize_t pos = 0;
        while (pos < len) {
            written = write(outfd, buff.data() + pos, len - pos);
            if (written > 0) {
                pos += written;
            } else if (errno != EINTR) {
                return -1;
            }
        }
    }

    if (len == -1) {
        return -1;
    }

    return 0;
}

int copyat_using_rename(
        int src_dirfd, FilePath src, int dest_dirfd, FilePath dest, mode_t mode) noexcept {
    FileDescriptor src_fd{openat(src_dirfd, src, O_RDONLY | O_CLOEXEC)};
    if (not src_fd.is_open()) {
        return -1;
    }

    try {
        constexpr uint suffix_len = 10;
        InplaceBuff<PATH_MAX> tmp_dest{std::in_place, dest, '.'};
        tmp_dest.resize(tmp_dest.size + suffix_len + 1); // +1 for trailing '\0'
        tmp_dest[--tmp_dest.size] = '\0';
        auto opt_fd = create_unique_file(dest_dirfd, tmp_dest.data(), tmp_dest.size,
                suffix_len, O_WRONLY | O_CLOEXEC, mode);
        if (not opt_fd) {
            return -1; // errno is already set
        }

        FileDescriptor& dest_fd = *opt_fd;
        CallInDtor tmp_dest_remover = [&] {
            (void)unlinkat(dest_dirfd, tmp_dest.to_cstr().data(), 0);
        };

        if (blast(src_fd, dest_fd)) {
            return -1;
        }
        (void)dest_fd.close(); // Close file descriptor before renaming to avoid
                               // racy ETXTBSY if someone will try execute it
                               // before we close dest_fd
        if (renameat(dest_dirfd, tmp_dest.to_cstr().data(), dest_dirfd, dest)) {
            return -1;
        }
        // Success
        tmp_dest_remover.cancel();
        return 0;
    } catch (...) {
        errno = ENOMEM;
        return -1;
    }
}

int copyat(int src_dirfd, FilePath src, int dest_dirfd, FilePath dest, mode_t mode) noexcept {
    constexpr auto open_flags = O_WRONLY | O_TRUNC | O_CLOEXEC;
    FileDescriptor out(openat(dest_dirfd, dest, open_flags | O_CREAT | O_EXCL, mode));
    bool file_created = true;
    if (not out.is_open() and errno == EEXIST) {
        file_created = false;
        out = openat(dest_dirfd, dest, open_flags);
    }
    if (not out.is_open()) {
        if (errno != ETXTBSY) {
            return -1;
        }
        return copyat_using_rename(src_dirfd, src, dest_dirfd, dest, mode);
    }

    FileDescriptor in(openat(src_dirfd, src, O_RDONLY | O_CLOEXEC));
    if (not in.is_open()) {
        return -1;
    }
    if (blast(in, out)) {
        return -1;
    }

    if (not file_created) {
        off64_t offset = lseek64(out, 0, SEEK_CUR);
        if (offset == static_cast<decltype(offset)>(-1)) {
            return -1;
        }
        if (ftruncate64(out, offset)) {
            return -1;
        }
        if (fchmod(out, mode)) {
            return -1;
        }
    }
    return 0;
}

int copyat_using_rename(int src_dirfd, FilePath src, int dest_dirfd, FilePath dest) noexcept {
    struct stat64 sb {};
    if (fstatat64(src_dirfd, src, &sb, 0)) {
        return -1;
    }
    return copyat_using_rename(src_dirfd, src, dest_dirfd, dest, sb.st_mode);
}

int copyat(int src_dirfd, FilePath src, int dest_dirfd, FilePath dest) noexcept {
    struct stat64 sb {};
    if (fstatat64(src_dirfd, src, &sb, 0)) {
        return -1;
    }

    return copyat(src_dirfd, src, dest_dirfd, dest, sb.st_mode);
}

static uint current_process_threads_num() {
    auto opt = str2num<uint>(ProcStatFileContents::get(getpid()).field(19));
    assert(opt.has_value());
    return *opt;
}

void thread_fork_safe_copyat(
        int src_dirfd, FilePath src, int dest_dirfd, FilePath dest, mode_t mode) {
    STACK_UNWINDING_MARK;
    if (current_process_threads_num() == 1) {
        if (copyat(src_dirfd, src, dest_dirfd, dest, mode)) {
            THROW("copyat()", errmsg());
        }
        return;
    }

    FileDescriptor efd{eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)};
    if (not efd.is_open()) {
        THROW("eventfd()", errmsg());
    }

    pid_t child = fork();
    if (child == -1) {
        THROW("fork()", errmsg());
    }
    if (child == 0) {
        if (copyat(src_dirfd, src, dest_dirfd, dest, mode)) {
            uint64_t data = errno;
            if (write(efd, &data, sizeof(data)) != sizeof(data)) {
                _exit(2);
            }
            _exit(1);
        }
        _exit(0);
    }
    // Parent process
    siginfo_t si;
    if (syscalls::waitid(P_PID, child, &si, WEXITED, nullptr) == -1) {
        THROW("waitid()", errmsg());
    }
    if (si.si_code != CLD_EXITED or (si.si_status | 1) != 1) {
        THROW("copying within child process failed");
    }
    if (si.si_status == 0) {
        return; // Success
    }

    uint64_t data{};
    if (read(efd, &data, sizeof(data)) != sizeof(data)) {
        THROW("read()", errmsg());
    }
    THROW("copy()", errmsg(data));
}

/**
 * @brief Copies directory @p src to @p dest relative to the directory file
 *   descriptors
 *
 * @param src_dirfd directory file descriptor
 * @param src source directory (relative to @p src_dirfd)
 * @param dest_dirfd directory file descriptor
 * @param dest destination directory (relative to @p dest_dirfd)
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for fstat64(2), openat(2), fdopendir(3),
 *   mkdirat(2), copyat()
 */
static int copy_rat_impl(int src_dirfd, FilePath src, int dest_dirfd, FilePath dest) noexcept {
    int src_fd = openat(src_dirfd, src, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (src_fd == -1) {
        if (errno == ENOTDIR) {
            return copyat(src_dirfd, src, dest_dirfd, dest);
        }

        return -1;
    }

    // Do not use src permissions
    mkdirat(dest_dirfd, dest, S_0755);

    int dest_fd = openat(dest_dirfd, dest, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dest_fd == -1) {
        close(src_fd);
        return -1;
    }

    DIR* src_dir = fdopendir(src_fd);
    if (src_dir == nullptr) {
        close(src_fd);
        close(dest_fd);
        return -1;
    }

    int ec = 0;
    int rc = 0;
    for_each_dir_component(
            src_dir,
            [&](dirent* file) -> repeating {
#ifdef _DIRENT_HAVE_D_TYPE
                if (file->d_type == DT_DIR || file->d_type == DT_UNKNOWN) {
#endif
                    if (copy_rat_impl(src_fd, file->d_name, dest_fd, file->d_name)) {
                        ec = errno;
                        rc = -1;
                        return stop_repeating;
                    }

#ifdef _DIRENT_HAVE_D_TYPE
                } else {
                    if (copyat(src_fd, file->d_name, dest_fd, file->d_name)) {
                        ec = errno;
                        rc = -1;
                        return stop_repeating;
                    }
                }
#endif

                return continue_repeating;
            },
            [&] {
        ec = errno;
        rc = -1;
            });

    closedir(src_dir);
    close(dest_fd);

    if (rc == -1) {
        errno = ec;
    }

    return rc;
}

int copy_rat(int src_dirfd, FilePath src, int dest_dirfd, FilePath dest) noexcept {
    struct stat64 sb {};
    if (fstatat64(src_dirfd, src, &sb, 0) == -1) {
        return -1;
    }

    if (S_ISDIR(sb.st_mode)) {
        return copy_rat_impl(src_dirfd, src, dest_dirfd, dest);
    }

    return copyat(src_dirfd, src, dest_dirfd, dest, sb.st_mode & ACCESSPERMS);
}

int copy_r(FilePath src, FilePath dest, bool create_subdirs) noexcept {
    if (create_subdirs and create_subdirectories(CStringView(dest)) == -1) {
        return -1;
    }

    return copy_rat(AT_FDCWD, src, AT_FDCWD, dest);
}

int move(FilePath oldpath, FilePath newpath, bool create_subdirs) noexcept {
    if (create_subdirs and create_subdirectories(CStringView(newpath)) == -1) {
        return -1;
    }

    if (rename(oldpath, newpath) == -1) {
        if (errno == EXDEV && copy_r(oldpath, newpath, false) == 0) {
            return remove_r(oldpath);
        }

        return -1;
    }

    return 0;
}

int create_file(FilePath pathname, mode_t mode) noexcept {
    int fd = open(pathname, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode);
    if (fd == -1) {
        return -1;
    }

    return close(fd);
}
