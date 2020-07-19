#pragma once

#include "simlib/debug.hh"
#include "simlib/file_path.hh"
#include "simlib/time.hh"

#include <chrono>
#include <sys/stat.h>
#include <unistd.h>

[[nodiscard]] inline int access(FilePath pathname, int mode) noexcept {
	return access(pathname.data(), mode);
}

inline bool path_exists(FilePath path) { return (access(path, F_OK) == 0); }

/**
 * @brief Check whether @p file exists and is a regular file
 *
 * @param file path of the file to check (has to be null-terminated)
 *
 * @return true if @p file is a regular file, false otherwise. To distinguish
 *   other file type error from stat64(2) error set errno to 0 before calling
 *   this function, if stat64(2) fails, errno will have nonzero value
 */
inline bool is_regular_file(FilePath file) noexcept {
	struct stat64 st {};
	return (stat64(file, &st) == 0 && S_ISREG(st.st_mode));
}

/**
 * @brief Check whether @p file exists and is a directory
 *
 * @param file path of the file to check (has to be null-terminated)
 *
 * @return true if @p file is a directory, false otherwise. To distinguish
 *   other file type error from stat64(2) error set errno to 0 before calling
 *   this function, if stat64(2) fails, errno will have nonzero value
 */
inline bool is_directory(FilePath file) noexcept {
	struct stat64 st {};
	return (stat64(file, &st) == 0 && S_ISDIR(st.st_mode));
}

inline uint64_t get_file_size(FilePath file) {
	struct stat64 st {};
	if (stat64(file, &st)) {
		THROW("stat()", errmsg());
	}

	return st.st_size;
}

// Returns file modification time (with second precision) as a time_point
inline std::chrono::system_clock::time_point
get_modification_time(const struct stat64& st) noexcept {
	return std::chrono::system_clock::time_point(to_duration(st.st_mtim));
}

// Returns file modification time (with second precision) as a time_point
inline std::chrono::system_clock::time_point
get_modification_time(FilePath file) {
	struct stat64 st {};
	if (stat64(file, &st)) {
		THROW("stat()", errmsg());
	}

	return get_modification_time(st);
}
