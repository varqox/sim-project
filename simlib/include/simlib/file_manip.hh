#pragma once

#include <climits>
#include <fcntl.h>
#include <simlib/file_path.hh>
#include <simlib/file_perms.hh>
#include <simlib/inplace_buff.hh>
#include <sys/stat.h>
#include <unistd.h>

// The same as unlink(const char*)
[[nodiscard]] inline int unlink(FilePath pathname) noexcept { return unlink(pathname.data()); }

// The same as remove(const char*)
[[nodiscard]] inline int remove(FilePath pathname) noexcept { return remove(pathname.data()); }

/**
 * @brief Removes recursively file/directory @p pathname relative to the
 *   directory file descriptor @p dirfd
 *
 * @param dirfd directory file descriptor
 * @param pathname file/directory pathname (relative to @p dirfd)
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for openat(2), unlinkat(2), fdopendir(3)
 */
[[nodiscard]] int remove_rat(int dirfd, FilePath pathname) noexcept;

/**
 * @brief Removes recursively file/directory @p pathname
 * @details Uses remove_rat()
 *
 * @param pathname file/directory to remove
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for remove_rat()
 */
[[nodiscard]] inline int remove_r(FilePath pathname) noexcept {
    return remove_rat(AT_FDCWD, pathname);
}

// Create directory (not recursively) (mode: 0755/rwxr-xr-x)
[[nodiscard]] inline int mkdir(FilePath pathname) noexcept { return mkdir(pathname, S_0755); }

// Create directories recursively (default mode: 0755/rwxr-xr-x)
[[nodiscard]] int mkdir_r(std::string path, mode_t mode = S_0755) noexcept;

/**
 * @brief Removes recursively all the contents of the directory @p pathname
 *   relative to the directory file descriptor @p dirfd
 * @details Uses remove_rat()
 *
 * @param dirfd directory file descriptor
 * @param pathname directory pathname (relative to @p dirfd)
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for remove_rat()
 */
[[nodiscard]] int remove_dir_contents_at(int dirfd, FilePath pathname) noexcept;

/**
 * @brief Removes recursively all the contents of the directory @p pathname
 * @details Uses remove_rat()
 *
 * @param pathname path to the directory
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for remove_rat()
 */
[[nodiscard]] inline int remove_dir_contents(FilePath pathname) noexcept {
    return remove_dir_contents_at(AT_FDCWD, pathname);
}

/**
 * @brief Creates directories containing @p file if they don't exist
 *
 * @param file path to file for which create enclosing directories
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for mkdir_r()
 */
[[nodiscard]] int create_subdirectories(FilePath file) noexcept;

/**
 * @brief Fast copies file from @p infd to @p outfd
 * @details Reads from @p infd form it's offset and writes to @p outfd from its
 *   offset
 *
 * @param infd file descriptor from which data will be copied
 * @param outfd file descriptor to which data will be copied
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for read(2), write(2)
 */
[[nodiscard]] int blast(int infd, int outfd) noexcept;

/**
 * @brief Copies (overrides) file @p src to @p dest relative to a directory
 *   file descriptor
 *
 * @param src_dirfd directory file descriptor
 * @param src source file (relative to @p src_dirfd)
 * @param dest_dirfd directory file descriptor
 * @param dest destination file (relative to @p dest_dirfd)
 * @param mode access mode to set for the destination file
 *
 * @return 0 on success, -1 on error (errno is set respectively)
 */
[[nodiscard]] int copyat_using_rename(
    int src_dirfd, FilePath src, int dest_dirfd, FilePath dest, mode_t mode
) noexcept;

/**
 * @brief Copies (overrides) file @p src to @p dest relative to a directory
 *   file descriptor
 *
 * @param src_dirfd directory file descriptor
 * @param src source file (relative to @p src_dirfd)
 * @param dest_dirfd directory file descriptor
 * @param dest destination file (relative to @p dest_dirfd)
 * @param mode access mode to set for the destination file (will be set iff
 *   the file is created)
 *
 * @return 0 on success, -1 on error (errno is set respectively)
 */
[[nodiscard]] int
copyat(int src_dirfd, FilePath src, int dest_dirfd, FilePath dest, mode_t mode) noexcept;

/**
 * @brief Copies (overrides) file @p src to @p dest relative to a directory
 *   file descriptor
 *
 * @param src_dirfd directory file descriptor
 * @param src source file (relative to @p src_dirfd)
 * @param dest_dirfd directory file descriptor
 * @param dest destination file (relative to @p dest_dirfd)
 *
 * @return 0 on success, -1 on error (errno is set respectively)
 */
[[nodiscard]] int
copyat_using_rename(int src_dirfd, FilePath src, int dest_dirfd, FilePath dest) noexcept;

/**
 * @brief Copies (overrides) file @p src to @p dest relative to a directory
 *   file descriptor
 *
 * @param src_dirfd directory file descriptor
 * @param src source file (relative to @p src_dirfd)
 * @param dest_dirfd directory file descriptor
 * @param dest destination file (relative to @p dest_dirfd)
 *
 * @return 0 on success, -1 on error (errno is set respectively)
 */
[[nodiscard]] int copyat(int src_dirfd, FilePath src, int dest_dirfd, FilePath dest) noexcept;

/**
 * @brief Copies (overwrites) file from @p src to @p dest
 *
 * @param src source file
 * @param dest destination file
 * @param mode access mode to set for the destination file
 *
 * @return 0 on success, -1 on error (errno is set respectively)
 */
[[nodiscard]] inline int copy_using_rename(FilePath src, FilePath dest, mode_t mode) noexcept {
    return copyat_using_rename(AT_FDCWD, src, AT_FDCWD, dest, mode);
}

/**
 * @brief Copies (overwrites) file from @p src to @p dest
 *
 * @param src source file
 * @param dest destination file
 *
 * @return 0 on success, -1 on error (errno is set respectively)
 */
[[nodiscard]] inline int copy_using_rename(FilePath src, FilePath dest) noexcept {
    return copyat_using_rename(AT_FDCWD, src, AT_FDCWD, dest);
}

/**
 * @brief Copies (overwrites) file from @p src to @p dest
 * @details Needs directory containing @p dest to exist
 *
 * @param src source file
 * @param dest destination file
 * @param mode access mode of the destination file (will be set iff the file is
 *   created)
 *
 * @return 0 on success, -1 on error (errno is set respectively)
 */
[[nodiscard]] inline int copy(FilePath src, FilePath dest, mode_t mode) noexcept {
    return copyat(AT_FDCWD, src, AT_FDCWD, dest, mode);
}

/**
 * @brief Copies (overwrites) file from @p src to @p dest
 * @details Needs directory containing @p dest to exist
 *
 * @param src source file
 * @param dest destination file
 *
 * @return 0 on success, -1 on error (errno is set respectively)
 */
[[nodiscard]] inline int copy(FilePath src, FilePath dest) noexcept {
    return copyat(AT_FDCWD, src, AT_FDCWD, dest);
}

/**
 * Copy function that protects from a nasty race condition that may result when
 * running in a multi-threaded program that uses fork(). Race scenario is as
 * follows:
 * - [thread 1] open file X for writing
 * - [thread 2] fork()
 * - [thread 1] close file X
 * - [thread 1] execve(file X) results in ETXTBSY, because [new process from
 *              thread 2] holds the file X open for writing
 * - [new process from thread 2] exit
 * This function avoids this problem by opening the file X in a child process,
 * thus preventing the race.
 */
void thread_fork_safe_copyat(
    int src_dirfd, FilePath src, int dest_dirfd, FilePath dest, mode_t mode
);

/**
 * Copy function that protects from a nasty race condition that may result when
 * running in a multi-threaded program that uses fork(). Race scenario is as
 * follows:
 * - [thread 1] open file X for writing
 * - [thread 2] fork()
 * - [thread 1] close file X
 * - [thread 1] execve(file X) results in ETXTBSY, because [new process from
 *              thread 2] holds the file X open for writing
 * - [new process from thread 2] exit
 * This function avoids this problem by opening the file X in a child process,
 * thus preventing the race.
 */
inline void thread_fork_safe_copy(FilePath src, FilePath dest, mode_t mode) {
    return thread_fork_safe_copyat(AT_FDCWD, src, AT_FDCWD, dest, mode);
}

/**
 * @brief Copies (overrides) file/directory @p src to @p dest relative to a
 *   directory file descriptor
 *
 * @param src_dirfd directory file descriptor
 * @param src source file/directory (relative to @p src_dirfd)
 * @param dest_dirfd directory file descriptor
 * @param dest destination file/directory (relative to @p dest_dirfd)
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for fstat64(2), openat(2), fdopendir(3),
 *   mkdirat(2), copyat()
 */
[[nodiscard]] int copy_rat(int src_dirfd, FilePath src, int dest_dirfd, FilePath dest) noexcept;

/**
 * @brief Copies (overrides) recursively files and folders
 * @details Uses copy_rat()
 *
 * @param src source file/directory
 * @param dest destination file/directory
 * @param create_subdirs whether create subdirectories or not
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for copy_rat()
 */
[[nodiscard]] int copy_r(FilePath src, FilePath dest, bool create_subdirs = true) noexcept;

[[nodiscard]] inline int rename(FilePath source, FilePath destination) noexcept {
    return rename(source.data(), destination.data());
}

[[nodiscard]] inline int link(FilePath source, FilePath destination) noexcept {
    return link(source.data(), destination.data());
}

/**
 * @brief Moves file from @p oldpath to @p newpath
 * @details First creates directory containing @p newpath
 *   (if @p create_subdirs is true) and then uses rename(2) to move
 *   file/directory or copy_r() and remove_r() if rename(2) fails with
 *   errno == EXDEV
 *
 * @param oldpath path to file/directory
 * @param newpath location
 * @param create_subdirs whether create @p newpath subdirectories or not
 *
 * @return Return value of rename(2) or copy_r() or remove_r()
 */
[[nodiscard]] int move(FilePath oldpath, FilePath newpath, bool create_subdirs = true) noexcept;

/**
 * @brief Creates file pathname with access mode @p mode
 *
 * @param pathname pathname for a file
 * @param mode access mode
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for creat(2), close(2)
 */
[[nodiscard]] int create_file(FilePath pathname, mode_t mode = S_0644) noexcept;

template <int (*func)(FilePath)>
class RemoverBase {
    InplaceBuff<PATH_MAX> name;

public:
    RemoverBase() = default;

    RemoverBase(const RemoverBase&) = delete;
    RemoverBase& operator=(const RemoverBase&) = delete;
    RemoverBase(const RemoverBase&&) = delete;
    RemoverBase& operator=(const RemoverBase&&) = delete;

    explicit RemoverBase(FilePath str) : RemoverBase(str.data(), str.size()) {}

    /// If @p str is null then @p len is ignored
    RemoverBase(const char* str, size_t len) : name(len + 1) {
        if (len != 0) {
            strncpy(name.data(), str, len + 1);
        }
        name.size = len;
    }

    ~RemoverBase() {
        if (name.size != 0) {
            (void)func(name);
        }
    }

    void cancel() noexcept { name.size = 0; }

    void reset(FilePath str) { reset(str.data(), str.size()); }

    void reset(const char* str, size_t len) {
        cancel();
        if (len != 0) {
            name.lossy_resize(len + 1);
            strncpy(name.data(), str, len + 1);
            name.size = len;
        }
    }

    [[nodiscard]] int remove_target() noexcept {
        if (name.size == 0) {
            return 0;
        }

        int rc = 0;
        rc = func(name);
        cancel();
        return rc;
    }
};

using FileRemover = RemoverBase<unlink>;
using DirectoryRemover = RemoverBase<remove_r>;
