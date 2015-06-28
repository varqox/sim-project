#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

/**
 * @brief Creates unlinked temporary file
 * @details Uses mkstemp(3)
 *
 * @param templ the same as template in mkstemp(3)
 * @return file descriptor on success, -1 on error
 */
int getUnlinkedTmpFile(const std::string& templ = "tmp_fileXXXXXX");

class TemporaryDirectory
{
private:
	std::string path; // absolute path
	char* name_;
	TemporaryDirectory(const TemporaryDirectory&);
	TemporaryDirectory& operator=(const TemporaryDirectory&);

public:
	explicit TemporaryDirectory(const char* templ);

	~TemporaryDirectory();

	const char* name() const { return name_; }

	std::string sname() const { return name_; }
};

// creates directory (not recursively) (mode: 0755/rwxr-xr-x)
inline int mkdir(const char* pathname) {
	return mkdir(pathname, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
}

// creates directory (not recursively) (mode: 0755/rwxr-xr-x)
inline int mkdir(const std::string& pathname) {
	return mkdir(pathname.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH |
		S_IXOTH);
}

// creates directories recursively (default mode: 0755/rwxr-xr-x)
int mkdir_r(const char* pathname, mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP |
		S_IROTH | S_IXOTH);

// creates directories recursively (default mode: 0755/rwxr-xr-x)
inline int mkdir_r(const std::string& pathname, mode_t mode = S_IRWXU |
		S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) {
	return mkdir_r(pathname.c_str(), mode);
}

/**
 * @brief Removes recursively file/directory @p pathname relative to a directory
 * file descriptor @p dirfd
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
 * @brief Copies (overrides) file from @p src to @p dest
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
 * @details Needs directory containing @p dest to exist
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
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for copy_rat()
 */
inline int copy_r(const std::string& src, const std::string& dest) {
	return copy_r(src.c_str(), dest.c_str());
}

/**
 * @brief Copies (overrides) recursively files and folders
 * @details Uses copy_rat()
 *
 * @param src source file/directory
 * @param dest destination file/directory
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for copy_rat()
 */
int copy_r(const char* src, const char* dest);

inline int access(const std::string& pathname, int mode) {
	return access(pathname.c_str(), mode);
}

inline int move(const std::string& oldpath, const std::string& newpath) {
	size_t x = newpath.find_last_of('/');
	if (x != std::string::npos)
		mkdir_r(newpath.substr(0, x).c_str());
	return rename(oldpath.c_str(), newpath.c_str());
}

namespace directory_tree {

// Node is a directory
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
	 * @return pointer to subdirectory or NULL if not exits
	 */
	node* dir(const std::string& pathname);

	/**
	 * @brief Checks if file exists in this node
	 *
	 * @param pathname file to check (cannot contain '/')
	 * @return if exists true, else false
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
	 * @param stream file to which write (if NULL returns immediately)
	 */
	inline void print(FILE *stream) {
		if (stream != NULL)
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


/* Returns an absolute path that does not contain any . or .. components,
*  nor any repeated path separators (/) nor / at end
*  root can be empty
*/
std::string abspath(const std::string& path, size_t beg = 0,
		size_t end = std::string::npos, std::string root = "/");

// returns extension (with dot) e.g. ".cc"
inline std::string getExtension(const std::string file) {
	return file.substr(file.find_last_of('.') + 1);
}

/**
 * @brief Reads until end of file
 *
 * @param fd file descriptor to read from
 *
 * @return read contents
 *
 * @errors The same that occur for read(2)
 */
std::string getFileContents(int fd);

/**
 * @brief Reads form @p fd from beg to end
 *
 * @param fd file descriptor to read from
 * @param beg begin offset
 * @param end end offset (@p end < 0 means size of file)
 *
 * @return read contents
 *
 * @errors The same that occur for lseek64(3), read(2)
 */
std::string getFileContents(int fd, off64_t beg, off64_t end = -1);

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

int putFileContents(const char* file, const char* data, size_t len = -1);

inline int putFileContents(const std::string& file, const std::string& data) {
	return putFileContents(file.c_str(), data.c_str(), data.size());
}
