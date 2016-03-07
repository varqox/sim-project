#include "../include/filesystem.h"
#include "../include/logger.h"
#include "../include/process.h"

#include <dirent.h>

using std::array;
using std::string;
using std::vector;

int getUnlinkedTmpFile(int flags) noexcept {
	int fd;
#ifdef O_TMPFILE
	fd = open("/tmp", O_TMPFILE | O_RDWR | flags, S_0600);
	if (fd != -1)
		return fd;

	if (errno != EINVAL)
		return -1;
#endif

	char name[] = "/tmp/tmp_unlinked_file.XXXXXX";
	umask(077); // Only owner can access this temporary file
	fd = mkostemp(name, flags);
	if (fd == -1)
		return -1;
	(void)unlink(name);
	return fd;
}

TemporaryDirectory::TemporaryDirectory(const char* templ) {
	size_t size = strlen(templ);
	if (size > 0) {
		// Fill name_
		if (templ[size - 1] == '/')
			--size;

		name_.reset(new char[size + 2]);

		memcpy(name_.get(), templ, size);
		name_.get()[size] = name_.get()[size + 1] = '\0';

		// Create directory with permissions (mode: 0700/rwx------)
		if (mkdtemp(name_.get()) == nullptr)
			throw std::runtime_error("Cannot create temporary directory\n");

		// name_ is absolute
		if (name_.get()[0] == '/')
			path_ = name();
		// name_ is not absolute
		else
			path_ = concat(getCWD(), name());

		// Make path_ absolute
		abspath(path_).swap(path_);
		path_ += '/';

		name_.get()[size] = '/';
	}
}

TemporaryDirectory::~TemporaryDirectory() {
	if (path_.size() && remove_r(path_) == -1)
		errlog("Error: remove_r()", error(errno)); // TODO: it is not so good
}

int mkdir_r(const char* path, mode_t mode) {
	string dir(path);
	// Remove ending slash (if it exists)
	if (dir.size() && dir.back() == '/')
		dir.pop_back();

	size_t end = 0;
	int res;
	do {
		end = find(dir, '/', end + 1);
		res = mkdir(dir.substr(0, end).c_str(), mode);

		if (res == -1 && errno != EEXIST)
			return -1;

	} while (end < dir.size());

	return res;
}

/**
 * @brief Removes recursively directory @p pathname relative to a directory file
 *   descriptor @p dirfd
 *
 * @param dirfd directory file descriptor
 * @param pathname directory pathname (relative to @p dirfd)
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for fstatat64(2), openat(2), unlinkat(2),
 *   fdopendir(3)
 */
int __remove_rat(int dirfd, const char* path) {
	int fd = openat(dirfd, path, O_RDONLY | O_LARGEFILE | O_DIRECTORY
		| O_NOFOLLOW);
	if (fd == -1)
		return unlinkat(dirfd, path, AT_REMOVEDIR);

	DIR *dir = fdopendir(fd);
	if (dir == nullptr) {
		sclose(fd);
		return unlinkat(dirfd, path, AT_REMOVEDIR);
	}

	dirent *file;
	while ((file = readdir(dir)))
		if (0 != strcmp(file->d_name, ".") && 0 != strcmp(file->d_name, "..")) {
			if (file->d_type == DT_DIR)
				__remove_rat(fd, file->d_name);
			else
				unlinkat(fd, file->d_name, 0);
		}

	closedir(dir);
	return unlinkat(dirfd, path, AT_REMOVEDIR);
}

int remove_rat(int dirfd, const char* path) {
	struct stat64 sb;
	if (fstatat64(dirfd, path, &sb, AT_SYMLINK_NOFOLLOW) == -1)
		return -1;

	if (S_ISDIR(sb.st_mode))
		return __remove_rat(dirfd, path);

	return unlinkat(dirfd, path, 0);
}

int blast(int infd, int outfd) {
	array<char, 65536> buff;
	ssize_t len, written;
	while (len = read(infd, buff.data(), buff.size()), len > 0 ||
			(len == -1 && errno == EINTR)) {
		ssize_t pos = 0;
		while (pos < len) {
			if ((written = write(outfd, buff.data() + pos, len - pos)) == -1)
				return -1;
			pos += written;
		}
	}
	return 0;
}

int copy(const char* src, const char* dest) {
	int in = open(src, O_RDONLY | O_LARGEFILE);
	if (in == -1)
		return -1;

	int out = open(dest, O_WRONLY | O_CREAT | O_TRUNC, S_0644);
	if (out == -1) {
		sclose(in);
		return -1;
	}

	int res = blast(in, out);
	sclose(in);
	sclose(out);
	return res;
}

int copyat(int dirfd1, const char* src, int dirfd2, const char* dest) {
	int in = openat(dirfd1, src, O_RDONLY | O_LARGEFILE);
	if (in == -1)
		return -1;

	int out = openat(dirfd2, dest, O_WRONLY | O_CREAT | O_TRUNC, S_0644);
	if (out == -1) {
		sclose(in);
		return -1;
	}

	int res = blast(in, out);
	sclose(in);
	sclose(out);
	return res;
}

/**
 * @brief Copies directory @p src to @p dest relative to a directory file
 *   descriptor
 *
 * @param dirfd1 directory file descriptor
 * @param src source directory (relative to dirfd1)
 * @param dirfd2 directory file descriptor
 * @param dest destination directory (relative to dirfd2)
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for fstat64(2), openat(2), fdopendir(3),
 *   mkdirat(2), copyat()
 */
static int __copy_rat(int dirfd1, const char* src, int dirfd2,
		const char* dest) {
	int src_fd = openat(dirfd1, src, O_RDONLY |	O_LARGEFILE | O_DIRECTORY);
	if (src_fd == -1)
		return -1;

	// Do not use src permissions
	mkdirat(dirfd2, dest, S_0755);

	int dest_fd = openat(dirfd2, dest, O_RDONLY | O_LARGEFILE | O_DIRECTORY);
	if (dest_fd == -1) {
		sclose(src_fd);
		return -1;
	}

	DIR *src_dir = fdopendir(src_fd);
	if (src_dir == nullptr) {
		sclose(src_fd);
		sclose(dest_fd);
		return -1;
	}

	dirent *file;
	while ((file = readdir(src_dir)))
		if (0 != strcmp(file->d_name, ".") && 0 != strcmp(file->d_name, "..")) {
			if (file->d_type == DT_DIR)
				__copy_rat(src_fd, file->d_name, dest_fd, file->d_name);
			else
				copyat(src_fd, file->d_name, dest_fd, file->d_name);
		}

	closedir(src_dir);
	sclose(dest_fd);
	return 0;
}

int copy_rat(int dirfd1, const char* src, int dirfd2, const char* dest) {
	struct stat64 sb;
	if (fstatat64(dirfd1, src, &sb, AT_SYMLINK_NOFOLLOW) == -1)
		return -1;

	if (S_ISDIR(sb.st_mode))
		return __copy_rat(dirfd1, src, dirfd2, dest);

	return copyat(dirfd1, src, dirfd2, dest);
}

int copy_r(const char* src, const char* dest, bool create_subdirs) {
	string tmp(dest);
	// Extract containing directory
	size_t pos = tmp.find_last_of('/');
	if (pos < tmp.size())
		tmp.resize(pos);

	if (create_subdirs)
		mkdir_r(tmp.c_str()); // Ensure that exists

	return copy_rat(AT_FDCWD, src, AT_FDCWD, dest);
}

int move(const string& oldpath, const string& newpath, bool create_subdirs) {
	if (create_subdirs) {
		size_t x = newpath.find_last_of('/');
		if (x != string::npos)
			mkdir_r(newpath.substr(0, x).c_str());
	}

	if (rename(oldpath.c_str(), newpath.c_str()) == -1) {
		if (errno == EXDEV && copy_r(oldpath, newpath, false) == 0)
			return remove_r(oldpath.c_str());

		return -1;
	}

	return 0;
}

int createFile(const char* pathname, mode_t mode) {
	int fd = creat(pathname, mode);
	if (fd == -1)
		return -1;

	return sclose(fd);
}

size_t readAll(int fd, void *buf, size_t count) {
	ssize_t k;
	size_t pos = 0;
	uint8_t *buff = static_cast<uint8_t*>(buf);
	do {
		k = read(fd, buff + pos, count - pos);
		if (k > 0)
			pos += k;
		else if (k == 0) {
			errno = 0; // No error
			return pos;

		} else if (errno != EINTR)
			return pos; // Error

	} while (pos < count);

	errno = 0; // No error
	return count;
}

size_t writeAll(int fd, const void *buf, size_t count) {
	ssize_t k;
	size_t pos = 0;
	const uint8_t *buff = static_cast<const uint8_t*>(buf);
	do {
		k = write(fd, buff + pos, count - pos);
		if (k > 0)
			pos += k;
		else if (k == 0) {
			errno = 0; // No error
			return pos;

		} else if (errno != EINTR)
			return pos; // Error

	} while (pos < count);

	errno = 0; // No error
	return count;
}

namespace directory_tree {

void Node::__print(FILE *stream, string buff) {
	fprintf(stream, "%s%s/\n", buff.c_str(), name.c_str());

	// Update buffer
	if (buff.size() >= 4) {
		if (buff[buff.size() - 4] == '`')
			buff[buff.size() - 4] = ' ';
		buff.replace(buff.size() - 3, 2, "  ");
	}

	size_t dirs_len = dirs.size(), files_len = files.size();
	// Directories
	for (size_t i = 0; i < dirs_len; ++i)
		dirs[i]->__print(stream, buff +
				(i + 1 == dirs_len && files_len == 0 ? "`-- " : "|-- "));

	// Files
	for (size_t i = 0; i < files_len; ++i)
		fprintf(stream, "%s%c-- %s\n", buff.c_str(),
				(i + 1 == files_len ? '`' : '|'), files[i].c_str());
}

Node* Node::dir(const string& pathname) {
	if (dirs.empty())
		return nullptr;

	vector<Node*>::iterator down = dirs.begin(), up = --dirs.end(), mid;

	while (down < up) {
		mid = down + ((up - down) >> 1);

		if ((*mid)->name < pathname)
			down = ++mid;
		else
			up = mid;
	}

	return (*down)->name == pathname ? *down : nullptr;
}

static Node* __dumpDirectoryTreeAt(int dirfd, const char* path) {
	size_t len = strlen(path);
	if (len > 1 && path[len - 1] == '/')
		--len;

	Node *root = new Node(path, path + len);

	int fd = openat(dirfd, path, O_RDONLY | O_LARGEFILE | O_DIRECTORY);
	if (fd == -1)
		return root;

	DIR *dir = fdopendir(fd);
	if (dir == nullptr) {
		sclose(fd);
		return root;
	}

	dirent *file;
	while ((file = readdir(dir)))
		if (0 != strcmp(file->d_name, ".") && 0 != strcmp(file->d_name, "..")) {
			if (file->d_type == DT_DIR)
				root->dirs.push_back(__dumpDirectoryTreeAt(fd, file->d_name));
			else
				root->files.push_back(file->d_name);
		}

	closedir(dir);

	std::sort(root->dirs.begin(), root->dirs.end(),
		[](Node* a, Node* b) { return a->name < b->name; });
	std::sort(root->files.begin(), root->files.end());
	return root;
}

Node* dumpDirectoryTree(const char* path) {
	struct stat64 sb;
	if (stat64(path, &sb) == -1 ||
			!S_ISDIR(sb.st_mode))
		return nullptr;

	return __dumpDirectoryTreeAt(AT_FDCWD, path);
}

} // namespace directory_tree

string abspath(const string& path, size_t beg, size_t end, string curr_dir) {
	if (end > path.size())
		end = path.size();

	// path begin with '/'
	if (beg < end && path[beg] == '/')
		curr_dir = "/";

	while (beg < end) {
		while (beg < end && path[beg] == '/')
			++beg;

		size_t next_slash = std::min(end, find(path, '/', beg, end));

		// If [beg, next_slash) == "."
		if (next_slash - beg == 1 && path[beg] == '.') {
			beg = next_slash;
			continue;

		// If [beg, next_slash) == ".."
		} else if (next_slash - beg == 2 && path[beg] == '.' &&
				path[beg + 1] == '.') {
			// Erase last path component
			size_t new_size = curr_dir.size();

			while (new_size > 0 && curr_dir[new_size - 1] != '/')
				--new_size;
			// If updated curr_dir != "/" erase trailing '/'
			if (new_size > 1)
				--new_size;
			curr_dir.resize(new_size);

			beg = next_slash;
			continue;
		}

		if (curr_dir.size() && curr_dir.back() != '/')
			curr_dir += '/';

		curr_dir.append(path, beg, next_slash - beg);
		beg = next_slash;
	}

	return curr_dir;
}

string getFileContents(int fd, size_t bytes) {
	string res;
	array<char, 65536> buff;
	while (bytes > 0) {
		ssize_t len = read(fd, buff.data(), std::min(buff.size(), bytes));
		// Interrupted by signal
		if (len < 0 && errno == EINTR)
			continue;
		// EOF or error
		if (len <= 0)
			break;

		bytes -= len;
		res.append(buff.data(), len);
	}

	return res;
}

string getFileContents(int fd, off64_t beg, off64_t end) {
	array<char, 65536> buff;
	off64_t size = lseek64(fd, 0, SEEK_END);
	if (beg < 0)
		beg = std::max<off64_t>(size + beg, 0);
	if (size == (off64_t)-1 || beg > size)
		return "";

	// Change end to the valid value
	if (size < end || end < 0)
		end = size;

	if (end < beg)
		end = beg;
	// Reposition to beg
	if (beg != lseek64(fd, beg, SEEK_SET))
		return "";

	off64_t bytes_left = end - beg;
	string res;
	while (bytes_left > 0) {
		ssize_t len = read(fd, buff.data(),
			std::min<off64_t>(buff.size(), bytes_left));
		// Interrupted by signal
		if (len < 0 && errno == EINTR)
			continue;
		// EOF or error
		if (len <= 0)
			break;

		bytes_left -= len;
		res.append(buff.data(), len);
	}

	return res;
}

string getFileContents(const char* file) {
	int fd;
	while ((fd = open(file, O_RDONLY | O_LARGEFILE)) == -1 && errno == EINTR) {}

	if (fd == -1)
		return "";

	Closer closer(fd); // Exceptions can be thrown
	string res = getFileContents(fd);

	return res;
}

string getFileContents(const char* file, off64_t beg, off64_t end) {
	int fd;
	while ((fd = open(file, O_RDONLY | O_LARGEFILE)) == -1 && errno == EINTR) {}

	if (fd == -1)
		return "";

	Closer closer(fd); // Exceptions can be thrown
	string res = getFileContents(fd, beg, end);

	return res;
}

vector<string> getFileByLines(const char* file, int flags, size_t first,
		size_t last) {
	vector<string> res;

	FILE *f = fopen(file, "r");
	if (f == nullptr)
		return res;

	char *buff = nullptr;
	size_t n = 0, line = 0;
	ssize_t read;
	// TODO: getline fails on '\0' ??? - check it out
	while ((read = getline(&buff, &n, f)) != -1) {
		if ((flags & GFBL_IGNORE_NEW_LINES) && buff[read - 1] == '\n')
			buff[read - 1] = '\0';

		if (line >= first && line < last)
			try {
				res.push_back(buff);
			} catch (...) {
				fclose(f);
				free(buff);
				throw;
			}

		++line;
	}

	fclose(f);
	free(buff);

	return res;
}

ssize_t putFileContents(const char* file, const char* data, size_t len) {
	FILE *f = fopen(file, "w");
	if (f == nullptr)
		return -1;

	if (len == size_t(-1))
		len = strlen(data);

	size_t pos = 0, written = 1;

	while (pos < len && written != 0) {
		written = fwrite(data + pos, sizeof(char), len - pos, f);
		pos += written;
	}

	fclose(f);
	return pos;
}
