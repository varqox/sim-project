#pragma once

#include "memory.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

/**
 * @brief Creates unlinked temporary file
 * @details Uses open(3) if O_TMPFILE is defined, or mkstemp(3)
 *
 * @param flags flags which be ORed with O_TMPFILE | O_RDWR in open(2) or passed
 * to mkostemp(3)
 * @return file descriptor on success, -1 on error
 *
 * @errors The same that occur for open(2) (if O_TMPFILE is defined) or
 * mkstemp(3)
 */
int getUnlinkedTmpFile(int flags = 0) noexcept;

class TemporaryDirectory
{
private:
	std::string path_; // absolute path
	UniquePtr<char> name_;

	TemporaryDirectory(const TemporaryDirectory&) = delete;
	TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

public:
	TemporaryDirectory() = default;

	explicit TemporaryDirectory(const char* templ);

	TemporaryDirectory(TemporaryDirectory&& td) : path_(std::move(td.path_)),
		name_(td.name_.release()) {}

	TemporaryDirectory& operator=(TemporaryDirectory&& td) {
		path_ = std::move(td.path_);
		name_.reset(td.name_.release());

		return *this;
	}

	~TemporaryDirectory();

	// Directory name with trailing '/'
	const char* name() const { return name_.get(); }

	// Directory name with trailing '/'
	std::string sname() const { return name_.get(); }

	// Directory absolute path with trailing '/'
	std::string path() const { return path_; }
};

// Create directory (not recursively) (mode: 0755/rwxr-xr-x)
inline int mkdir(const char* pathname) {
	return mkdir(pathname, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
}

// Create directory (not recursively) (mode: 0755/rwxr-xr-x)
inline int mkdir(const std::string& pathname) {
	return mkdir(pathname.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH |
		S_IXOTH);
}

// Create directories recursively (default mode: 0755/rwxr-xr-x)
int mkdir_r(const char* pathname, mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP |
		S_IROTH | S_IXOTH);

// Create directories recursively (default mode: 0755/rwxr-xr-x)
inline int mkdir_r(const std::string& pathname, mode_t mode = S_IRWXU |
		S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) {
	return mkdir_r(pathname.c_str(), mode);
}

/**
 * @brief Removes recursively file/directory @p pathname relative to the
 * directory file descriptor @p dirfd
 *
 * @param dirfd directory file descriptor
 * @param pathname file/directory pathname (relative to @p dirfd)
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for fstatat64(2), openat(2), unlinkat(2),
 * fdopendir(3)
 */
int remove_rat(int dirfd, const char* pathname);

/**
 * @brief Removes recursively file/directory @p pathname
 * @details Uses remove_rat()
 *
 * @param pathname file/directory to remove
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for remove_rat()
 */
inline int remove_r(const char* pathname) {
	return remove_rat(AT_FDCWD, pathname);
}

// The same as remove_r(const char*)
inline int remove_r(const std::string& pathname) {
	return remove_rat(AT_FDCWD, pathname.c_str());
}

/**
 * @brief Fast copies file from @p infd to @p outfd
 * @details Reads from @p infd form it's offset and writes to @p outfd from its
 * offset
 *
 * @param infd file descriptor from which data will be copied
 * @param outfd file descriptor to which data will be copied
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for read(2), write(2)
 */
int blast(int infd, int outfd);

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
int copy(const char* src, const char* dest);

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
inline int copy(const std::string& src, const std::string& dest) {
	return copy(src.c_str(), dest.c_str());
}

/**
 * @brief Copies (overrides) file @p src to @p dest relative to a directory file
 * descriptor
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
int copyat(int dirfd1, const char* src, int dirfd2, const char* dest);

/**
 * @brief Copies (overrides) file/directory @p src to @p dest relative to a
 * directory file descriptor
 *
 * @param dirfd1 directory file descriptor
 * @param src source file/directory (relative to @p dirfd1)
 * @param dirfd2 directory file descriptor
 * @param dest destination file/directory (relative to @p dirfd2)
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for fstat64(2), openat(2), fdopendir(3),
 * mkdirat(2), copyat()
 */
int copy_rat(int dirfd1, const char* src, int dirfd2, const char* dest);

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
int copy_r(const char* src, const char* dest, bool create_subdirs = true);

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
		bool create_subdirs = true) {
	return copy_r(src.c_str(), dest.c_str(), create_subdirs);
}

inline int access(const std::string& pathname, int mode) {
	return access(pathname.c_str(), mode);
}

/**
 * @brief Moves file from @p oldpath to @p newpath
 * @details First creates directory containing @p newpath
 * (if @p create_subdirs is true) and then uses rename(2) to move file/directory
 * or copy_r() and remove_r() if rename(2) fails with errno == EXDEV
 *
 * @param oldpath path to file/directory
 * @param newpath location
 * @param create_subdirs whether create @p newpath subdirectories or not
 *
 * @return Return value of rename(2) or copy_r() or remove_r()
 */
int move(const std::string& oldpath, const std::string& newpath,
		bool create_subdirs = true);

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
int createFile(const char* pathname, mode_t mode);

namespace directory_tree {

// Node represents a directory
class node {
private:
	node(const node&);
	node& operator=(const node&);

	/**
	 * @brief Prints tree rooted in *this
	 *
	 * @param stream file to which write (cannot be NULL - does not check that)
	 * @param buff buffer used to printing tree structure
	 */
	void __print(FILE *stream, std::string buff = "");

public:
	std::string name;
	std::vector<node*> dirs;
	std::vector<std::string> files;

	explicit node(const std::string& new_name)
			: name(new_name), dirs(), files() {}

	template<class Iter>
	node(Iter beg, Iter end) : name(beg, end), dirs(), files() {}

	/**
	 * @brief Get subdirectory
	 *
	 * @param pathname name to search (cannot contain '/')
	 * @return pointer to subdirectory or NULL if it does not exist
	 */
	node* dir(const std::string& pathname);

	/**
	 * @brief Checks if file exists in this node
	 *
	 * @param pathname file to check (cannot contain '/')
	 * @return true if exists, false otherwise
	 */
	bool fileExists(const std::string& pathname) {
		return std::binary_search(files.begin(), files.end(), pathname);
	}

	~node() {
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

	/**
	 * @brief Compares two nodes (given via pointer) by name
	 *
	 * @param a
	 * @param b
	 *
	 * @return a->name < b->name
	 */
	static bool cmp(node* a, node* b) { return a->name < b->name; }
};

/**
 * @brief Dumps directory tree (rooted in @p path)
 *
 * @param path path to main directory
 * @return pointer to root node
 */
node* dumpDirectoryTree(const char* path);

/**
 * @brief Dumps directory tree (rooted in @p path)
 *
 * @param path path to main directory
 * @return pointer to root node
 */
inline node* dumpDirectoryTree(const std::string& path) {
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
 * @param beg begin offset
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
		off64_t end = -1) {
	return getFileContents(file.c_str(), beg, end);
}

const int GFBL_IGNORE_NEW_LINES = 1; // Erase '\n' from each line
/**
 * @brief Get file contents by lines in range [first, last)
 *
 * @param file filename
 * @param flags if set GFBL_IGNORE_NEW_LINES then '\n' is not appended to each
 * line
 * @param first number of first line to fetch
 * @param last number of first line not to fetch
 *
 * @return vector<string> containing fetched lines
 */
std::vector<std::string> getFileByLines(const char* file, int flags = 0,
	size_t first = 0, size_t last = -1);

inline std::vector<std::string> getFileByLines(const std::string& file,
		int flags = 0, size_t first = 0, size_t last = -1) {
	return getFileByLines(file.c_str(), flags, first, last);
}

/**
 * @brief Writes @p data to file @p file
 * @details Writes all data or nothing
 *
 * @param file file to write to
 * @param data data to write
 *
 * @return bytes written on success, -1 on error
 */
ssize_t putFileContents(const char* file, const char* data, size_t len = -1);

/**
 * @brief Writes @p data to file @p file
 * @details Writes all data or nothing
 *
 * @param file file to write to
 * @param data data to write
 *
 * @return bytes written on success, -1 on error
 */
inline ssize_t putFileContents(const std::string& file,
		const std::string& data) {
	return putFileContents(file.c_str(), data.c_str(), data.size());
}

/**
 * @brief Behaves like close(2) but cannot be interrupted by signal
 *
 * @param fd file descriptor to close
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
	Closer(int fd) : fd_(fd) {}

	void cancel() { fd_ = -1; }

	/**
	 * @brief Closes file descriptor
	 * @return 0 on success, -1 on error
	 * @errors The same that occur to sclose()
	 */
	int close() noexcept {
		int rc = sclose(fd_);
		fd_ = -1;
		return rc;
	}

	~Closer() { sclose(fd_); }
};

// Encapsulates file descriptor
class Fd {
	int fd_;

public:
	explicit Fd(int fd = -1) : fd_(fd) {}

	Fd(Fd&&) = default;

	Fd& operator=(const Fd&) = default;

	Fd& operator=(Fd&&) = default;

	Fd& operator=(int fd) {
		fd_ = fd;
		return *this;
	}

	operator int() const { return fd_; }

	void reset(int fd) {
		sclose(fd_);
		fd_ = fd;
	}

	int open(const char* filename, int flags) {
		return fd_ = ::open(filename, flags);
	}

	int open(const char* filename, int flags, int mode) {
		return fd_ = ::open(filename, flags, mode);
	}

	int open(const std::string& filename, int flags) {
		return fd_ = ::open(filename.c_str(), flags);
	}

	int open(const std::string& filename, int flags, int mode) {
		return fd_ = ::open(filename.c_str(), flags, mode);
	}

	int reopen(const char* filename, int flags) {
		sclose(fd_);
		return fd_ = ::open(filename, flags);
	}

	int reopen(const char* filename, int flags, int mode) {
		sclose(fd_);
		return fd_ = ::open(filename, flags, mode);
	}

	int reopen(const std::string& filename, int flags) {
		sclose(fd_);
		return fd_ = ::open(filename.c_str(), flags);
	}

	int reopen(const std::string& filename, int flags, int mode) {
		sclose(fd_);
		return fd_ = ::open(filename.c_str(), flags, mode);
	}

	int close() {
		if (fd_ > 0) {
			int rc = sclose(fd_);
			fd_ = -1;
			return rc;
		}

		return 0;
	}

	~Fd() { sclose(fd_); }
};

template<int (*func)(const char*)>
class RemoverBase {
	UniquePtr<char> name;

	RemoverBase(const RemoverBase&) = delete;
	RemoverBase& operator=(const RemoverBase&) = delete;
	RemoverBase(const RemoverBase&&) = delete;
	RemoverBase& operator=(const RemoverBase&&) = delete;

public:
	explicit RemoverBase(const char* str) : RemoverBase(str, str == nullptr
		? 0 : strlen(str)) {}

	explicit RemoverBase(const std::string& str)
		: RemoverBase(str.data(), str.size()) {}

	RemoverBase(const char* str, size_t len) : name(nullptr) {
		if (str != nullptr) {
			name.reset(new char[len + 1]);
			strncpy(name.get(), str, len + 1);
		}
	}

	~RemoverBase() {
		if (!name.isNull())
			func(name.get());
	}

	void cancel() { name.reset(); }

	void reset(const char* str) { reset(str, strlen(str)); }

	void reset(const std::string& str) { reset(str.data(), str.size()); }

	void reset(const char* str, size_t len) {
		cancel();
		name.reset(new char[len + 1]);
		strncpy(name.get(), str, len + 1);
	}

	int removeTarget() {
		int rc = func(name.get());
		cancel();
		return rc;
	}
};

typedef RemoverBase<unlink> FileRemover;
typedef RemoverBase<remove_r> DirectoryRemover;
