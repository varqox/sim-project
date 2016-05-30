#pragma once

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

// File modes
constexpr int S_0600 = S_IRUSR | S_IWUSR;
constexpr int S_0644 = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
constexpr int S_0700 = S_IRWXU;
constexpr int S_0755 = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

/**
 * @brief Creates unlinked temporary file
 * @details Uses open(3) if O_TMPFILE is defined, or mkstemp(3)
 *
 * @param flags flags which be ORed with O_TMPFILE | O_RDWR in open(2) or passed
 *   to mkostemp(3)
 *
 * @return file descriptor on success, -1 on error
 *
 * @errors The same that occur for open(2) (if O_TMPFILE is defined) or
 *   mkstemp(3)
 */
int getUnlinkedTmpFile(int flags = 0) noexcept;

class TemporaryDirectory {
private:
	std::string path_; // absolute path
	std::unique_ptr<char[]> name_;

public:
	TemporaryDirectory() = default;

	explicit TemporaryDirectory(const char* templ);

	TemporaryDirectory(const TemporaryDirectory&) = delete;

	TemporaryDirectory(TemporaryDirectory&& td) noexcept
		: path_(std::move(td.path_)), name_(std::move(td.name_)) {}

	TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

	TemporaryDirectory& operator=(TemporaryDirectory&& td) noexcept {
		path_ = std::move(td.path_);
		name_ = std::move(td.name_);

		return *this;
	}

	~TemporaryDirectory();

	// Directory name with trailing '/'
	const char* name() const noexcept { return name_.get(); }

	// Directory name with trailing '/'
	std::string sname() const { return name_.get(); }

	// Directory absolute path with trailing '/'
	const std::string& path() const noexcept { return path_; }
};

// Create directory (not recursively) (mode: 0755/rwxr-xr-x)
inline int mkdir(const char* pathname) noexcept {
	return mkdir(pathname, S_0755);
}

// Create directory (not recursively) (mode: 0755/rwxr-xr-x)
inline int mkdir(const std::string& pathname) noexcept {
	return mkdir(pathname.c_str(), S_0755);
}

// Create directories recursively (default mode: 0755/rwxr-xr-x)
int mkdir_r(const char* pathname, mode_t mode = S_0755) noexcept;

// Create directories recursively (default mode: 0755/rwxr-xr-x)
inline int mkdir_r(const std::string& pathname, mode_t mode = S_0755) noexcept {
	return mkdir_r(pathname.c_str(), mode);
}

// The same as unlink(const char*)
inline int unlink(const std::string& pathname) noexcept {
	return unlink(pathname.c_str());
}

// The same as remove(const char*)
inline int remove(const std::string& pathname) noexcept {
	return remove(pathname.c_str());
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
 * @errors The same that occur for fstatat64(2), openat(2), unlinkat(2),
 *   fdopendir(3)
 */
int remove_rat(int dirfd, const char* pathname) noexcept;

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
inline int remove_r(const char* pathname) noexcept {
	return remove_rat(AT_FDCWD, pathname);
}

// The same as remove_r(const char*)
inline int remove_r(const std::string& pathname) noexcept {
	return remove_rat(AT_FDCWD, pathname.c_str());
}

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
 * @brief Copies (overwrites) file from @p src to @p dest
 * @details Needs directory containing @p dest to exist
 *
 * @param src source file
 * @param dest destination file
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for open(2), blast()
 */
int copy(const char* src, const char* dest) noexcept;

/**
 * @brief Copies (overrides) file from @p src to @p dest
 * @details Requires for a directory containing @p dest to exist
 *
 * @param src source file
 * @param dest destination file
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for open(2), blast()
 */
inline int copy(const std::string& src, const std::string& dest) noexcept {
	return copy(src.c_str(), dest.c_str());
}

/**
 * @brief Copies (overrides) file @p src to @p dest relative to a directory file
 *   descriptor
 *
 * @param dirfd1 directory file descriptor
 * @param src source file (relative to @p dirfd1)
 * @param dirfd2 directory file descriptor
 * @param dest destination file (relative to @p dirfd2)
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for openat(2), blast()
 */
int copyat(int dirfd1, const char* src, int dirfd2, const char* dest) noexcept;

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
int copy_rat(int dirfd1, const char* src, int dirfd2, const char* dest)
	noexcept;

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
int copy_r(const char* src, const char* dest, bool create_subdirs = true)
	noexcept;

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
inline int copy_r(const std::string& src, const std::string& dest,
	bool create_subdirs = true) noexcept
{
	return copy_r(src.c_str(), dest.c_str(), create_subdirs);
}

inline int access(const std::string& pathname, int mode) noexcept {
	return access(pathname.c_str(), mode);
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
int move(const std::string& oldpath, const std::string& newpath,
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
int createFile(const char* pathname, mode_t mode) noexcept;

/**
 * @brief Read @p count bytes to @p buf from @p fd
 * @details Uses read(2), but reads until it is unable to read
 *
 * @param fd file descriptor
 * @param buf where place read bytes
 * @param count number of bytes to read
 *
 * @return number of bytes read, if error occurs then errno is > 0
 *
 * @errors The same as for read(2) except EINTR
 */
size_t readAll(int fd, void *buf, size_t count) noexcept;

/**
 * @brief Write @p count bytes to @p fd from @p buf
 * @details Uses write(2), but writes until it is unable to write
 *
 * @param fd file descriptor
 * @param buf where write bytes from
 * @param count number of bytes to write
 *
 * @return number of bytes written, if error occurs then errno is > 0
 *
 * @errors The same as for write(2) except EINTR
 */
size_t writeAll(int fd, const void *buf, size_t count) noexcept;

namespace directory_tree {

// Node represents a directory
class Node {
private:
	/**
	 * @brief Prints tree rooted in *this
	 *
	 * @param stream file to which write (cannot be NULL - does not check that)
	 * @param buff buffer used to printing tree structure
	 */
	void __print(FILE *stream, std::string buff = "");

public:
	std::string name;
	std::vector<Node*> dirs;
	std::vector<std::string> files;

	explicit Node(std::string new_name)
			: name(new_name), dirs(), files() {}

	Node(const Node&) = delete;

	Node(Node&& n) noexcept : name(std::move(n.name)), dirs(std::move(n.dirs)),
		files(std::move(n.files)) {}

	Node& operator=(const Node&) = delete;

	Node& operator=(Node&& n) noexcept {
		for (size_t i = 0; i < dirs.size(); ++i)
			delete dirs[i];

		name = std::move(n.name);
		dirs = std::move(n.dirs);
		files = std::move(n.files);

		return *this;
	}

	template<class Iter>
	Node(Iter beg, Iter end) : name(beg, end), dirs(), files() {}

	/**
	 * @brief Get subdirectory
	 *
	 * @param pathname name to search (cannot contain '/')
	 *
	 * @return pointer to subdirectory or NULL if it does not exist
	 */
	Node* dir(const std::string& pathname);

	/**
	 * @brief Checks if file exists in this node
	 *
	 * @param pathname file to check (cannot contain '/')
	 *
	 * @return true if exists, false otherwise
	 */
	bool fileExists(const std::string& pathname) noexcept {
		return std::binary_search(files.begin(), files.end(), pathname);
	}

	~Node() {
		for (size_t i = 0; i < dirs.size(); ++i)
			delete dirs[i];
	}

	/**
	 * @brief Prints tree rooted in *this
	 *
	 * @param stream file to write to (if NULL returns immediately)
	 */
	inline void print(FILE *stream) {
		if (stream != nullptr)
			return __print(stream);
	}
};

/**
 * @brief Dumps directory tree (rooted in @p path)
 *
 * @param path path to main directory
 *
 * @return pointer to root node
 */
Node* dumpDirectoryTree(const char* path);

/**
 * @brief Dumps directory tree (rooted in @p path)
 *
 * @param path path to main directory
 *
 * @return pointer to root node
 */
inline Node* dumpDirectoryTree(const std::string& path) {
	return dumpDirectoryTree(path.c_str());
}

} // namespace directory_tree

/*
*  Returns an absolute path that does not contain any . or .. components,
*  nor any repeated path separators (/), and does not end with /
*  curr_dir can be empty. If path begin with / then curr_dir is ignored.
*/
std::string abspath(const std::string& path, size_t beg = 0,
		size_t end = std::string::npos, std::string curr_dir = "/");

// returns extension (with dot) e.g. ".cc"
inline std::string getExtension(const std::string file) {
	return file.substr(file.find_last_of('.') + 1);
}

/**
 * @brief Reads until end of file
 *
 * @param fd file descriptor to read from
 * @param bytes number of bytes to read
 *
 * @return read contents
 *
 * @errors The same that occur for read(2)
 */
std::string getFileContents(int fd, size_t bytes = -1);

/**
 * @brief Reads form @p fd from beg to end
 *
 * @param fd file descriptor to read from
 * @param beg begin offset (if negative, then is set to file_size + @p beg)
 * @param end end offset (@p end < 0 means size of file)
 *
 * @return read contents
 *
 * @errors The same that occur for lseek64(3), read(2)
 */
std::string getFileContents(int fd, off64_t beg, off64_t end);

/**
 * @brief Reads until end of file
 *
 * @param file file to read from
 *
 * @return read contents
 *
 * @errors The same that occur for open(2), read(2), close(2)
 */
std::string getFileContents(const char* file);

/**
 * @brief Reads form @p file from beg to end
 *
 * @param file file to read from
 * @param beg begin offset (if negative, then is set to file_size + @p beg)
 * @param end end offset (@p end < 0 means size of file)
 *
 * @return read contents
 *
 * @errors The same that occur for open(2), lseek64(3), read(2), close(2)
 */
std::string getFileContents(const char* file, off64_t beg, off64_t end = -1);

/**
 * Alias to getFileContents(const char*)
 */
inline std::string getFileContents(const std::string& file) {
	return getFileContents(file.c_str());
}

/**
 * Alias to getFileContents(const char*, off64_t, off64_t)
 */
inline std::string getFileContents(const std::string& file, off64_t beg,
	off64_t end = -1)
{
	return getFileContents(file.c_str(), beg, end);
}

constexpr int GFBL_IGNORE_NEW_LINES = 1; // Erase '\n' from each line
/**
 * @brief Get file contents by lines in range [first, last)
 *
 * @param file filename
 * @param flags if set GFBL_IGNORE_NEW_LINES then '\n' is not appended to each
 *   line
 * @param first number of first line to fetch
 * @param last number of first line not to fetch
 *
 * @return vector<string> containing fetched lines
 */
std::vector<std::string> getFileByLines(const char* file, int flags = 0,
	size_t first = 0, size_t last = -1);

inline std::vector<std::string> getFileByLines(const std::string& file,
	int flags = 0, size_t first = 0, size_t last = -1)
{
	return getFileByLines(file.c_str(), flags, first, last);
}

/**
 * @brief Writes @p data to file @p file
 * @details Writes all data or nothing
 *
 * @param file file to write to
 * @param data data to write
 *
 * @return bytes written, if error occurs then errno is > 0
 *
 * @errors The same that occur for open(2) and write(2) except EINTR from
 *   write(2)
 */
ssize_t putFileContents(const char* file, const char* data, size_t len = -1)
	noexcept;

/**
 * @brief Writes @p data to file @p file
 * @details Writes all data or nothing
 *
 * @param file file to write to
 * @param data data to write
 *
 * @return bytes written, if error occurs then errno is > 0
 *
 * @errors The same that occur for open(2) and write(2) except EINTR from
 *   write(2)
 */
inline ssize_t putFileContents(const std::string& file,
	const std::string& data) noexcept
{
	return putFileContents(file.c_str(), data.c_str(), data.size());
}

/**
 * @brief Behaves like close(2) but cannot be interrupted by signal
 *
 * @param fd file descriptor to close
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for close(2) expect EINTR
 */
inline int sclose(int fd) noexcept {
	while (close(fd) == -1)
		if (errno != EINTR)
			return -1;

	return 0;
}

// Closes file descriptor automatically
class Closer {
	int fd_;
public:
	explicit Closer(int fd) noexcept : fd_(fd) {}

	void cancel() noexcept { fd_ = -1; }

	/**
	 * @brief Closes file descriptor
	 *
	 * @return 0 on success, -1 on error
	 *
	 * @errors The same that occur to sclose()
	 */
	int close() noexcept {
		if (fd_ < 0)
			return 0;

		int rc = sclose(fd_);
		fd_ = -1;
		return rc;
	}

	~Closer() {
		if (fd_ >= 0)
			sclose(fd_);
	}
};

// Encapsulates file descriptor
class FileDescriptor {
	int fd_;

public:
	explicit FileDescriptor(int fd = -1) noexcept : fd_(fd) {}

	explicit FileDescriptor(const char* filename, int flags, int mode = S_0644)
		noexcept
		: fd_(::open(filename, flags, mode)) {}

	FileDescriptor(FileDescriptor&&) noexcept = default;

	FileDescriptor& operator=(const FileDescriptor&) noexcept = default;

	FileDescriptor& operator=(FileDescriptor&&) noexcept = default;

	FileDescriptor& operator=(int fd) noexcept {
		fd_ = fd;
		return *this;
	}

	operator int() const noexcept { return fd_; }

	void reset(int fd) noexcept {
		if (fd_ >= 0)
			(void)sclose(fd_);
		fd_ = fd;
	}

	int open(const char* filename, int flags, int mode = S_0644) noexcept {
		return fd_ = ::open(filename, flags, mode);
	}

	int open(const std::string& filename, int flags, int mode = S_0644) noexcept
	{
		return fd_ = ::open(filename.c_str(), flags, mode);
	}

	int reopen(const char* filename, int flags, int mode = S_0644) noexcept {
		reset(::open(filename, flags, mode));
		return fd_;
	}

	int reopen(const std::string& filename, int flags, int mode = S_0644)
		noexcept
	{
		reset(::open(filename.c_str(), flags, mode));
		return fd_;
	}

	int close() noexcept {
		if (fd_ < 0)
			return 0;

		int rc = sclose(fd_);
		fd_ = -1;
		return rc;
	}

	~FileDescriptor() {
		if (fd_ >= 0)
			(void)sclose(fd_);
	}
};

template<int (*func)(const char*)>
class RemoverBase {
	std::unique_ptr<char[]> name;

	RemoverBase(const RemoverBase&) = delete;
	RemoverBase& operator=(const RemoverBase&) = delete;
	RemoverBase(const RemoverBase&&) = delete;
	RemoverBase& operator=(const RemoverBase&&) = delete;

public:
	explicit RemoverBase(const char* str)
		: RemoverBase(str, (str ? strlen(str) : 0)) {}

	explicit RemoverBase(const std::string& str)
		: RemoverBase(str.data(), str.size()) {}

	RemoverBase(const char* str, size_t len) : name(nullptr) {
		if (str != nullptr) {
			name.reset(new char[len + 1]);
			strncpy(name.get(), str, len + 1);
		}
	}

	~RemoverBase() {
		if (name)
			func(name.get());
	}

	void cancel() noexcept { name.reset(); }

	void reset(const char* str) { reset(str, strlen(str)); }

	void reset(const std::string& str) { reset(str.data(), str.size()); }

	void reset(const char* str, size_t len) {
		cancel();
		name.reset(new char[len + 1]);
		strncpy(name.get(), str, len + 1);
	}

	int removeTarget() noexcept {
		int rc = func(name.get());
		cancel();
		return rc;
	}
};

typedef RemoverBase<unlink> FileRemover;
typedef RemoverBase<remove_r> DirectoryRemover;

/**
 * @brief Converts @p size, so that it human readable
 * @details It adds proper prefixes, for example:
 *   1023 -> "1023"
 *   1024 -> "1.0 KB"
 *   129747 -> "127 KB"
 *   97379112 -> "92.9 MB"
 *
 * @param size size to humanize
 * @return humanized file size
 */
std::string humanizeFileSize(uint64_t size);
