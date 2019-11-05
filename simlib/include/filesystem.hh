#pragma once

#include "debug.hh"
#include "string_traits.hh"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

// TODO: split this header file into many smaller ones

// File modes
constexpr int S_0600 = S_IRUSR | S_IWUSR;
constexpr int S_0644 = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
constexpr int S_0700 = S_IRWXU;
constexpr int S_0755 = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

// Encapsulates file descriptor
class FileDescriptor {
	int fd_;

public:
	explicit FileDescriptor(int fd = -1) noexcept : fd_(fd) {}

	explicit FileDescriptor(FilePath filename, int flags,
	                        mode_t mode = S_0644) noexcept
	   : fd_(::open(filename, flags, mode)) {}

	FileDescriptor(const FileDescriptor&) = delete;

	FileDescriptor(FileDescriptor&& fd) noexcept : fd_(fd.release()) {}

	FileDescriptor& operator=(const FileDescriptor&) = delete;

	FileDescriptor& operator=(FileDescriptor&& fd) noexcept {
		reset(fd.release());
		return *this;
	}

	FileDescriptor& operator=(int fd) noexcept {
		reset(fd);
		return *this;
	}

	bool is_open() const noexcept { return (fd_ >= 0); }

	// To check for validity opened() should be used
	explicit operator bool() const noexcept = delete;

	operator int() const noexcept { return fd_; }

	int release() noexcept {
		int fd = fd_;
		fd_ = -1;
		return fd;
	}

	void reset(int fd) noexcept {
		if (fd_ >= 0)
			(void)::close(fd_);
		fd_ = fd;
	}

	int open(FilePath filename, int flags, int mode = S_0644) noexcept {
		return fd_ = ::open(filename, flags, mode);
	}

	int reopen(FilePath filename, int flags, int mode = S_0644) noexcept {
		reset(::open(filename, flags, mode));
		return fd_;
	}

	int close() noexcept {
		if (fd_ < 0)
			return 0;

		int rc = ::close(fd_);
		fd_ = -1;
		return rc;
	}

	~FileDescriptor() {
		if (fd_ >= 0)
			(void)::close(fd_);
	}
};

// Encapsulates directory object DIR
class Directory {
	DIR* dir_;

public:
	explicit Directory(DIR* dir = nullptr) noexcept : dir_(dir) {}

	explicit Directory(FilePath pathname) noexcept : dir_(opendir(pathname)) {}

	Directory(const Directory&) = delete;

	Directory(Directory&& d) noexcept : dir_(d.release()) {}

	Directory& operator=(const Directory&) = delete;

	Directory& operator=(Directory&& d) noexcept {
		reset(d.release());
		return *this;
	}

	Directory& operator=(DIR* d) noexcept {
		reset(d);
		return *this;
	}

	bool is_open() const noexcept { return (dir_ != nullptr); }

	operator DIR*() const noexcept { return dir_; }

	void rewind() noexcept {
		if (is_open())
			rewinddir(dir_);
	}

	DIR* release() noexcept {
		DIR* d = dir_;
		dir_ = nullptr;
		return d;
	}

	void reset(DIR* d) noexcept {
		if (dir_)
			(void)closedir(dir_);
		dir_ = d;
	}

	void close() noexcept {
		if (dir_) {
			(void)closedir(dir_);
			dir_ = nullptr;
		}
	}

	Directory& reopen(FilePath pathname) noexcept {
		reset(opendir(pathname));
		return *this;
	}

	~Directory() {
		if (dir_)
			(void)closedir(dir_);
	}
};

// The same as unlink(const char*)
inline int unlink(FilePath pathname) noexcept {
	return unlink(pathname.data());
}

// The same as remove(const char*)
inline int remove(FilePath pathname) noexcept {
	return remove(pathname.data());
}

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
int remove_rat(int dirfd, FilePath pathname) noexcept;

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
inline int remove_r(FilePath pathname) noexcept {
	return remove_rat(AT_FDCWD, pathname);
}

/**
 * @brief Creates (and opens) unlinked temporary file
 * @details Uses open(2) if O_TMPFILE is defined, or mkostemp(3)
 *
 * @param flags flags which be ORed with O_TMPFILE | O_RDWR in open(2) or
 *   passed to mkostemp(3)
 *
 * @return file descriptor on success, -1 on error
 *
 * @errors The same that occur for open(2) (if O_TMPFILE is defined) or
 *   mkostemp(3)
 */
int open_unlinked_tmp_file(int flags = 0) noexcept;

class TemporaryDirectory {
private:
	std::string path_; // absolute path
	std::unique_ptr<char[]> name_;

public:
	TemporaryDirectory() = default; // Does NOT create a temporary directory

	explicit TemporaryDirectory(FilePath templ);

	TemporaryDirectory(const TemporaryDirectory&) = delete;
	TemporaryDirectory(TemporaryDirectory&&) noexcept = default;
	TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

	TemporaryDirectory& operator=(TemporaryDirectory&& td) {
		if (exists() && remove_r(path_) == -1)
			THROW("remove_r() failed", errmsg());

		path_ = std::move(td.path_);
		name_ = std::move(td.name_);
		return *this;
	}

	~TemporaryDirectory();

	// Returns true if object holds a real temporary directory
	bool exists() const noexcept { return (name_.get() != nullptr); }

	// Directory name (from constructor parameter) with trailing '/'
	const char* name() const noexcept { return name_.get(); }

	// Directory name (from constructor parameter) with trailing '/'
	std::string sname() const { return name_.get(); }

	// Directory absolute path with trailing '/'
	const std::string& path() const noexcept { return path_; }
};

// Create directory (not recursively) (mode: 0755/rwxr-xr-x)
inline int mkdir(FilePath pathname) noexcept { return mkdir(pathname, S_0755); }

// Create directories recursively (default mode: 0755/rwxr-xr-x)
int mkdir_r(std::string path, mode_t mode = S_0755) noexcept;

class TemporaryFile {
	std::string path_; // absolute path

public:
	/// Does NOT create a temporary file
	TemporaryFile() = default;

	/// The last six characters of template must be "XXXXXX" and these are
	/// replaced with a string that makes the filename unique.
	explicit TemporaryFile(std::string templ) {
		throw_assert(has_suffix(templ, "XXXXXX") &&
		             "this is needed by mkstemp");
		FileDescriptor fd(mkstemp(templ.data()));
		if (not fd.is_open())
			THROW("mkstemp() failed", errmsg());
		path_ = std::move(templ);
	}

	TemporaryFile(const TemporaryFile&) = delete;
	TemporaryFile& operator=(const TemporaryFile&) = delete;
	TemporaryFile(TemporaryFile&&) noexcept = default;

	TemporaryFile& operator=(TemporaryFile&& tf) noexcept {
		if (is_open())
			unlink(path_);

		path_ = std::move(tf.path_);
		return *this;
	}

	~TemporaryFile() {
		if (is_open())
			unlink(path_);
	}

	bool is_open() const noexcept { return not path_.empty(); }

	const std::string& path() const noexcept { return path_; }
};

class OpenedTemporaryFile {
	std::string path_; // absolute path
	FileDescriptor fd_;

public:
	/// Does NOT create a temporary file
	OpenedTemporaryFile() = default;

	/// The last six characters of template must be "XXXXXX" and these are
	/// replaced with a string that makes the filename unique.
	explicit OpenedTemporaryFile(std::string templ) {
		throw_assert(has_suffix(templ, "XXXXXX") &&
		             "this is needed by mkstemp");
		fd_ = mkstemp(templ.data());
		if (not fd_.is_open())
			THROW("mkstemp() failed", errmsg());
		path_ = std::move(templ);
	}

	OpenedTemporaryFile(const OpenedTemporaryFile&) = delete;
	OpenedTemporaryFile& operator=(const OpenedTemporaryFile&) = delete;
	OpenedTemporaryFile(OpenedTemporaryFile&&) noexcept = default;

	OpenedTemporaryFile& operator=(OpenedTemporaryFile&& tf) noexcept {
		if (is_open())
			unlink(path_);

		path_ = std::move(tf.path_);
		fd_ = std::move(tf.fd_);

		return *this;
	}

	~OpenedTemporaryFile() {
		if (is_open())
			unlink(path_);
	}

	operator const FileDescriptor&() const { return fd_; }

	operator int() const { return fd_; }

	bool is_open() const noexcept { return fd_.is_open(); }

	const std::string& path() const noexcept { return path_; }
};

class DirectoryChanger {
	FileDescriptor old_cwd;

public:
	// Does NOT change current working directory
	DirectoryChanger() = default;

	explicit DirectoryChanger(FilePath new_wd)
	   : old_cwd(".", O_RDONLY | O_CLOEXEC) {
		if (old_cwd == -1)
			THROW("open() failed", errmsg());

		if (chdir(new_wd.data()) == -1) {
			auto err = errno;
			old_cwd.close();
			THROW("chdir() failed", errmsg(err));
		}
	}

	DirectoryChanger(const DirectoryChanger&) = delete;
	DirectoryChanger(DirectoryChanger&&) noexcept = default;
	DirectoryChanger& operator=(const DirectoryChanger&) = delete;
	DirectoryChanger& operator=(DirectoryChanger&&) noexcept = default;

	~DirectoryChanger() {
		if (old_cwd >= 0)
			(void)fchdir(old_cwd);
	}
};

/**
 * @brief Calls @p func on every component of the @p dir other than "." and
 *   ".."
 *
 * @param dir path to directory or directory object, readdir(3) is used on it so
     one may want to save its pos via telldir(3) and use seekdir(3) after the
     call or just rewinddir(3) after the call
 * @param func function to call on every component (other than "." and ".."),
 *   it should take one argument - dirent*, if it return sth convertible to
 *   false the lookup will break
 */
template <class DirType, class Func, class ErrFunc>
void for_each_dir_component(DirType&& dir, Func&& func,
                            ErrFunc&& readdir_failed) {
	static_assert(std::is_convertible_v<DirType, FilePath> or
	              std::is_convertible_v<DirType, DIR*>);
	static_assert(std::is_invocable_r_v<bool, Func, dirent*> or
	              std::is_invocable_v<Func, dirent*>);

	if constexpr (not std::is_convertible_v<DirType, DIR*>) {
		Directory directory {dir};
		if (!directory)
			THROW("opendir()", errmsg());

		return for_each_dir_component(directory, std::forward<Func>(func),
		                              std::forward<ErrFunc>(readdir_failed));

	} else {
		dirent* file;
		for (;;) {
			errno = 0;
			file = readdir(dir);
			if (file == nullptr) {
				if (errno == 0)
					return; // No more entries

				readdir_failed();
				return;
			}

			if (strcmp(file->d_name, ".") and strcmp(file->d_name, "..")) {
				if constexpr (std::is_constructible_v<decltype(func(file)),
				                                      bool>) {
					if (not func(file))
						return;
				} else {
					func(file);
				}
			}
		}
	}
}

template <class DirType, class Func>
auto for_each_dir_component(DirType&& dir, Func&& func) {
	static_assert(std::is_convertible_v<DirType, FilePath> or
	              std::is_convertible_v<DirType, DIR*>);
	static_assert(std::is_invocable_r_v<bool, Func, dirent*> or
	              std::is_invocable_v<Func, dirent*>);
	return for_each_dir_component(std::forward<DirType>(dir),
	                              std::forward<Func>(func),
	                              [] { THROW("readdir()", errmsg()); });
}

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
int remove_dir_contents_at(int dirfd, FilePath pathname) noexcept;

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
inline int remove_dir_contents(FilePath pathname) noexcept {
	return remove_dir_contents_at(AT_FDCWD, pathname);
}

/// @brief Creates directories containing @p file if they don't exist
int create_subdirectories(FilePath file) noexcept;

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
int blast(int infd, int outfd) noexcept;

/**
 * @brief Copies (overrides) file @p src to @p dest relative to a directory
 *   file descriptor
 *
 * @param dirfd1 directory file descriptor
 * @param src source file (relative to @p dirfd1)
 * @param dirfd2 directory file descriptor
 * @param dest destination file (relative to @p dirfd2)
 * @param mode access mode of the destination file (will be set iff the file is
 *   created)
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for openat(2), lseek(2), ftruncate(2),
 *   blast()
 */
int copyat(int dirfd1, FilePath src, int dirfd2, FilePath dest,
           mode_t mode = S_0644) noexcept;

/**
 * @brief Copies (overwrites) file from @p src to @p dest
 * @details Needs directory containing @p dest to exist
 *
 * @param src source file
 * @param dest destination file
 * @param mode access mode of the destination file (will be set iff the file is
 *   created)
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for copyat()
 */
inline int copy(FilePath src, FilePath dest, mode_t mode = S_0644) noexcept {
	return copyat(AT_FDCWD, src, AT_FDCWD, dest, mode);
}

/**
 * @brief Copies (overrides) file/directory @p src to @p dest relative to a
 *   directory file descriptor
 *
 * @param dirfd1 directory file descriptor
 * @param src source file/directory (relative to @p dirfd1)
 * @param dirfd2 directory file descriptor
 * @param dest destination file/directory (relative to @p dirfd2)
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for fstat64(2), openat(2), fdopendir(3),
 *   mkdirat(2), copyat()
 */
int copy_rat(int dirfd1, FilePath src, int dirfd2, FilePath dest) noexcept;

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
int copy_r(FilePath src, FilePath dest, bool create_subdirs = true) noexcept;

inline int access(FilePath pathname, int mode) noexcept {
	return access(pathname.data(), mode);
}

inline int rename(FilePath source, FilePath destination) noexcept {
	return rename(source.data(), destination.data());
}

inline int link(FilePath source, FilePath destination) noexcept {
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
int move(FilePath oldpath, FilePath newpath,
         bool create_subdirs = true) noexcept;

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
int create_file(FilePath pathname, mode_t mode = S_0644) noexcept;

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
size_t read_all(int fd, void* buff, size_t count) noexcept;

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
size_t write_all(int fd, const void* buff, size_t count) noexcept;

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
	if (write_all(fd, buff, count) != count)
		THROW("write()", errmsg());
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
inline size_t write_all(int fd, StringView str) noexcept {
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

template <int (*func)(FilePath)>
class RemoverBase {
	InplaceBuff<PATH_MAX> name;

	RemoverBase(const RemoverBase&) = delete;
	RemoverBase& operator=(const RemoverBase&) = delete;
	RemoverBase(const RemoverBase&&) = delete;
	RemoverBase& operator=(const RemoverBase&&) = delete;

public:
	RemoverBase() : name() {}

	explicit RemoverBase(FilePath str) : RemoverBase(str.data(), str.size()) {}

	/// If @p str is null then @p len is ignored
	RemoverBase(const char* str, size_t len) : name(len + 1) {
		if (len != 0)
			strncpy(name.data(), str, len + 1);
		name.size = len;
	}

	~RemoverBase() {
		if (name.size != 0)
			func(name);
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

	int remove_target() noexcept {
		if (name.size == 0)
			return 0;

		int rc = 0;
		rc = func(name);
		cancel();
		return rc;
	}
};

typedef RemoverBase<unlink> FileRemover;
typedef RemoverBase<remove_r> DirectoryRemover;

/**
 * @brief Converts @p size, so that it human readable
 * @details It adds proper suffixes, for example:
 *   1 -> "1 byte"
 *   1023 -> "1023 bytes"
 *   1024 -> "1.0 KiB"
 *   129747 -> "127 KiB"
 *   97379112 -> "92.9 MiB"
 *
 * @param size size to humanize
 *
 * @return humanized file size
 */
std::string humanize_file_size(uint64_t size);

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
	struct stat64 st;
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
	struct stat64 st;
	return (stat64(file, &st) == 0 && S_ISDIR(st.st_mode));
}

inline uint64_t get_file_size(FilePath file) {
	struct stat64 st;
	if (stat64(file, &st))
		THROW("stat()", errmsg());

	return st.st_size;
}

// Returns file modification time (with second precision) as a time_point
inline std::chrono::system_clock::time_point
get_modification_time(const struct stat64& st) noexcept {
	return std::chrono::system_clock::from_time_t(st.st_mtime);
}

// Returns file modification time (with second precision) as a time_point
inline std::chrono::system_clock::time_point
get_modification_time(FilePath file) {
	struct stat64 st;
	if (stat64(file, &st))
		THROW("stat()", errmsg());

	return get_modification_time(st);
}
