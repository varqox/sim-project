#pragma once

#include "simlib/debug.hh"
#include "simlib/file_path.hh"
#include "simlib/file_perms.hh"

#include <sys/types.h>

/**
 * @brief Read @p count bytes to @p buff from @p fd
 * @details Uses read(2), but reads until it is unable to read
 *
 * @param fd file descriptor
 * @param buff where place read bytes
 * @param count number of bytes to read
 *
 * @return number of bytes read, if error occurs then errno is > 0
 *
 * @errors The same as for read(2) except EINTR
 */
[[nodiscard]] size_t read_all(int fd, void* buff, size_t count) noexcept;

/**
 * @brief Read up to @p count bytes to @p buff from @p fd starting at @p pos
 *   without moving the file offset
 * @details Uses pread(), but reads until it is unable to read
 *
 * @param fd file descriptor
 * @param pos file offset -- where to start reading
 * @param buff where to place read bytes
 * @param count number of bytes to read
 *
 * @return number of bytes read, if error occurs then errno is > 0; note that
 *   a successful call may read less bytes than @p count e.g. because an EOF was
 *   encountered
 *
 * @errors The same as for pread() except EINTR
 */
[[nodiscard]] size_t pread_all(int fd, off64_t pos, void* buff,
                               size_t count) noexcept;

/**
 * @brief Write @p count bytes to @p fd from @p buff
 * @details Uses write(2), but writes until it is unable to write
 *
 * @param fd file descriptor
 * @param buff where write bytes from
 * @param count number of bytes to write
 *
 * @return number of bytes written, if error occurs then errno is > 0
 *
 * @errors The same as for write(2) except EINTR
 */
[[nodiscard]] size_t write_all(int fd, const void* buff, size_t count) noexcept;

/**
 * @brief Write @p count bytes to @p fd from @p str
 * @details Uses write(2), but writes until it is unable to write
 *
 * @param fd file descriptor
 * @param buff where write bytes from
 * @param count number of bytes to write
 *
 * @errors If any error occurs then an exception is thrown
 */
inline void write_all_throw(int fd, const void* buff, size_t count) {
	if (write_all(fd, buff, count) != count) {
		THROW("write()", errmsg());
	}
}

/**
 * @brief Write @p count bytes to @p fd from @p str
 * @details Uses write(2), but writes until it is unable to write
 *
 * @param fd file descriptor
 * @param str where write bytes from
 *
 * @return number of bytes written, if error occurs then errno is > 0
 *
 * @errors The same as for write(2) except EINTR
 */
[[nodiscard]] inline size_t write_all(int fd, StringView str) noexcept {
	return write_all(fd, str.data(), str.size());
}

/**
 * @brief Write @p count bytes to @p fd from @p str
 * @details Uses write(2), but writes until it is unable to write
 *
 * @param fd file descriptor
 * @param str where write bytes from
 *
 * @errors If any error occurs then an exception is thrown
 */
inline void write_all_throw(int fd, StringView str) {
	write_all_throw(fd, str.data(), str.size());
}

/**
 * @brief Reads until end of file
 *
 * @param fd file descriptor to read from
 * @param bytes number of bytes to read
 *
 * @return read contents
 *
 * @errors If any error occurs an exception of type std::runtime_error is
 *   thrown (may happen if read(2) fails)
 */
std::string get_file_contents(int fd, size_t bytes = -1);

/**
 * @brief Reads form @p fd from beg to end
 *
 * @param fd file descriptor to read from
 * @param beg begin offset (if negative, then is set to file_size + @p beg)
 * @param end end offset (@p end < 0 means size of file)
 *
 * @return read contents
 *
 * @errors If any error occurs an exception of type std::runtime_error is
 *   thrown (may happen if lseek64(3) or read(2) fails)
 */
std::string get_file_contents(int fd, off64_t beg, off64_t end);

/**
 * @brief Reads until end of file
 *
 * @param file file to read from
 *
 * @return read contents
 *
 * @errors If any error occurs an exception of type std::runtime_error is
 *   thrown (may happen if open(2), read(2) or close(2) fails)
 */
std::string get_file_contents(FilePath file);

/**
 * @brief Reads form @p file from beg to end
 *
 * @param file file to read from
 * @param beg begin offset (if negative, then is set to file_size + @p beg)
 * @param end end offset (@p end < 0 means size of file)
 *
 * @return read contents
 *
 * @errors If any error occurs an exception of type std::runtime_error is
 *   thrown (may happen if open(2), lseek64(3), read(2) or close(2) fails)
 */
std::string get_file_contents(FilePath file, off64_t beg, off64_t end = -1);

/**
 * @brief Writes @p data to file @p file
 * @details Writes all data or nothing
 *
 * @param file file to write to
 * @param data data to write
 * @param mode access mode
 *
 * @errors If any error occurs an exception of type std::runtime_error is
 *   thrown (may happen if open(2) or write(2) fails)
 */
void put_file_contents(FilePath file, const char* data, size_t len,
                       mode_t mode = S_0644);

inline void put_file_contents(FilePath file, StringView data) {
	return put_file_contents(file, data.data(), data.size());
}
