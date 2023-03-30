#pragma once

#include <fcntl.h>
#include <optional>
#include <simlib/debug.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/string_traits.hh>
#include <unistd.h>

class TemporaryFile {
    std::string path_; // absolute path

public:
    /// Does NOT create a temporary file
    TemporaryFile() = default;

    /// The last six characters of template must be "XXXXXX" and these are
    /// replaced with a string that makes the filename unique.
    explicit TemporaryFile(std::string templ, mode_t mode = S_IRUSR | S_IWUSR /* -rw------- */);

    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;
    TemporaryFile(TemporaryFile&&) noexcept = default;

    TemporaryFile& operator=(TemporaryFile&& tf) noexcept {
        if (is_open()) {
            unlink(path_.c_str());
        }

        path_ = std::move(tf.path_);
        return *this;
    }

    ~TemporaryFile() {
        if (is_open()) {
            unlink(path_.c_str());
        }
    }

    [[nodiscard]] bool is_open() const noexcept { return not path_.empty(); }

    [[nodiscard]] const std::string& path() const noexcept { return path_; }
};

/**
 * @brief Tries to create a file with path @p path_buff with randomly replaced
 *   last @p suffix_len bytes. @p path_buff[@p buff_len] must equal '\0'.
 *   The suffix will be replaced by lowercase Latin letters.
 *
 * @param dirfd if pathname stored in @p path_buff is relative, then it will be
 *   interpreted relative to the directory referred by the file descriptor
 *   @p dirfd.
 * @param path_buff buffer in which path template is provided and to which
 *   created file's path will be stored
 * @param buff_len length of the buffer
 * @param suffix_len length of the suffix to replace randomly
 * @param open_flags flags that ORed with (O_EXCL | O_CREAT) will be passed to
 *   openat(2)
 * @param mode specifies the file mode bits be applied when a new file is
 *   created
 *
 * @return On success file descriptor to the created file is returned. On error,
 *   std::nullopt is returned.
 *
 * @errors On error (std::nullopt is returned) errno is set to:
 *   - EEXIST if the file could not be created after a finite number of tries,
 *       content of @p path_buff in range
 *       [@p buff_len - @p suffix_len, @p buff_len) is undefined in this case.
 *   - any other error returned by openat(2)
 */
std::optional<FileDescriptor> create_unique_file(
    int dirfd, char* path_buff, size_t buff_len, size_t suffix_len, int open_flags, mode_t mode
) noexcept;
