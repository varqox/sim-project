#include "../include/debug.h"
#include "../include/filesystem.h"
#include "../include/string.h"

#include <cerrno>
#include <dirent.h>
#include <stdexcept>

using std::string;
using std::vector;

int getUnlinkedTmpFile(const string& templ) {
	char name[templ.size() + 1];
	strcpy(name, templ.c_str());

	int fd = mkstemp(name);
	if (fd == -1)
		return -1;

	unlink(name);
	return fd;
}

TemporaryDirectory::TemporaryDirectory(const char* templ)
		: path(), name_(NULL) {
	size_t size = strlen(templ);
	if (size > 0) {
		// Fill name_
		if (templ[size - 1] == '/')
			--size;

		name_ = new char[size + 2];

		memcpy(name_, templ, size);
		name_[size] = name_[size + 1] = '\0';

		// Create directory
		if (NULL == mkdtemp(name_)) {
			delete[] name_;
			throw std::runtime_error("Cannot create temporary directory\n");
		}

		// If name_ is absolute
		if (name_[0] == '/')
			path = name_;

		// If name_ is not absolute
		else {
			char *buff = get_current_dir_name();
			if (buff == NULL || strlen(buff) == 0) {
				if (buff != NULL) // strlen(buff) == 0
					errno = ENOENT;

				delete[] name_;
				free(buff);

				throw std::runtime_error(
					string("Cannot get current working directory - ") +
					strerror(errno) + "\n");
			}

			path = buff;
			free(buff);

			path += '/';
			path += name_;
		}

		// Make path absolute
		abspath(path).swap(path);
		path += '/';

		name_[size] = '/';

		// Change permissions (mode: 0755/rwxr-xr-x)
		chmod(name_, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	}
}

TemporaryDirectory::~TemporaryDirectory() {
	if (-1 == remove_r(path.c_str()))
		eprintf("Error: remove_r() - %s\n", strerror(errno));
	delete[] name_;
}

int mkdir_r(const char* path, mode_t mode) {
	string dir(path);
	// Remove ending slash (if it exists)
	if (dir.size() && *--dir.end() == '/')
		dir.erase(--dir.end());

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
 * descriptor @p dirfd
 *
 * @param dirfd directory file descriptor
 * @param pathname directory pathname (relative to @p dirfd)
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for fstatat64(2), openat(2), unlinkat(2),
 * fdopendir(3)
 */
int __remove_rat(int dirfd, const char* path) {
	int fd = openat(dirfd, path, O_RDONLY | O_NOCTTY | O_NONBLOCK |
			O_LARGEFILE | O_DIRECTORY | O_NOFOLLOW);
	if (fd == -1)
		return unlinkat(dirfd, path, AT_REMOVEDIR);

	DIR *dir = fdopendir(fd);
	if (dir == NULL) {
		close(fd);
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
	const size_t buff_size = 1 << 16;
	char buff[buff_size];
	ssize_t len, written;
	while (len = read(infd, buff, buff_size), len > 0 ||
			(len == -1 && errno == EINTR)) {
		ssize_t pos = 0;
		while (pos < len) {
			if ((written = write(outfd, buff + pos, len - pos)) == -1)
				return -1;
			pos += written;
		}
	}
	return 0;
}

int copy(const char* src, const char* dest) {
	int in = open(src, O_RDONLY | O_LARGEFILE | O_NOFOLLOW);
	if (in == -1)
		return -1;

	int out = open(dest, O_WRONLY | O_CREAT | O_TRUNC,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); // (mode: 0644/rw-r--r--)
	if (out == -1) {
		close(in);
		return -1;
	}

	int res = blast(in, out);
	close(in);
	close(out);
	return res;
}

int copyat(int dirfd1, const char* src, int dirfd2, const char* dest) {
	int in = openat(dirfd1, src, O_RDONLY | O_LARGEFILE);
	if (in == -1)
		return -1;

	int out = openat(dirfd2, dest, O_WRONLY | O_CREAT | O_TRUNC,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); // (mode: 0644/rw-r--r--)
	if (out == -1) {
		close(in);
		return -1;
	}

	int res = blast(in, out);
	close(in);
	close(out);
	return res;
}

/**
 * @brief Copies directory @p src to @p dest relative to a directory file
 * descriptor
 *
 * @param dirfd1 directory file descriptor
 * @param src source directory (relative to dirfd1)
 * @param dirfd2 directory file descriptor
 * @param dest destination directory (relative to dirfd2)
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for fstat64(2), openat(2), fdopendir(3),
 * mkdirat(2), copyat()
 */
static int __copy_rat(int dirfd1, const char* src, int dirfd2, const char* dest) {
	int src_fd = openat(dirfd1, src, O_RDONLY | O_NOCTTY | O_NONBLOCK |
			O_LARGEFILE | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (src_fd == -1)
		return -1;

	// Do not use src permissions
	mkdirat(dirfd2, dest, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);

	int dest_fd = openat(dirfd2, dest, O_RDONLY | O_NOCTTY | O_NONBLOCK |
			O_LARGEFILE | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (dest_fd == -1) {
		close(src_fd);
		return -1;
	}

	DIR *src_dir = fdopendir(src_fd);
	if (src_dir == NULL) {
		close(src_fd);
		close(dest_fd);
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
	close(dest_fd);
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

int copy_r(const char* src, const char* dest) {
	string tmp(dest);
	// Extract containing directory
	size_t pos = tmp.find_last_of('/');
	if (pos < tmp.size())
		tmp.resize(pos);

	mkdir_r(tmp.c_str()); // Ensure that exists

	return copy_rat(AT_FDCWD, src, AT_FDCWD, dest);
}

int createFile(const char* pathname, mode_t mode) {
	int fd = creat(pathname, mode);
	if (fd == -1)
		return -1;

	while (close(fd) == -1)
		if (errno != EINTR)
			return -1;

	return 0;
}

namespace directory_tree {

void node::__print(FILE *stream, string buff) {
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

node* node::dir(const string& pathname) {
	vector<node*>::iterator down = dirs.begin(), up = --dirs.end(), mid;

	while (down < up) {
		mid = down + ((up - down) >> 1);

		if ((*mid)->name < pathname)
			down = ++mid;
		else
			up = mid;
	}

	return (*down)->name == pathname ? *down : NULL;
}

static node* __dumpDirectoryTreeAt(int dirfd, const char* path) {
	size_t len = strlen(path);
	if (len > 1 && path[len - 1] == '/')
		--len;

	node *root = new node(path, path + len);

	int fd = openat(dirfd, path, O_RDONLY | O_NOCTTY | O_NONBLOCK |
			O_LARGEFILE | O_DIRECTORY);
	if (fd == -1)
		return root;

	DIR *dir = fdopendir(fd);
	if (dir == NULL) {
		close(fd);
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

	std::sort(root->dirs.begin(), root->dirs.end(), node::cmp);
	std::sort(root->files.begin(), root->files.end());
	return root;
}

node* dumpDirectoryTree(const char* path) {
	struct stat64 sb;
	if (stat64(path, &sb) == -1 ||
			!S_ISDIR(sb.st_mode))
		return NULL;

	return __dumpDirectoryTreeAt(AT_FDCWD, path);
}

} // namespace directory_tree

string abspath(const string& path, size_t beg, size_t end, string root) {
	if (end > path.size())
		end = path.size();

	while (beg < end) {
		while (beg < end && path[beg] == '/')
			++beg;

		size_t next_slash = std::min(end, find(path, '/', beg, end));

		// If [beg, next_slash) == ("." or "..")
		if ((next_slash - beg == 1 && path[beg] == '.') ||
				(next_slash - beg == 2 && path[beg] == '.' &&
				path[beg + 1] == '.')) {
			beg = next_slash;
			continue;
		}
		if (root.size() > 0 && *--root.end() != '/')
			root += '/';

		root.append(path, beg, next_slash - beg);
		beg = next_slash;
	}

	return root;
}

string getFileContents(int fd) {
	const size_t buff_len = 1 << 16;
	char buff[buff_len];

	string res;
	ssize_t len;
	for (;;) {
		len = read(fd, buff, buff_len);
		// Interrupted by signal
		if (len < 0 && errno == EINTR)
			continue;
		// EOF or error
		if (len <= 0)
			break;

		res.append(buff, len);
	}

	return res;
}

string getFileContents(int fd, off64_t beg, off64_t end) {
	const size_t buff_len = 1 << 16;
	char buff[buff_len];

	off64_t size = lseek64(fd, 0, SEEK_END);
	if (size == (off64_t)-1 || beg < 0 || beg > size)
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
	ssize_t len;
	while (bytes_left > 0) {
		len = read(fd, buff, std::min<off64_t>(buff_len, bytes_left));
		// Interrupted by signal
		if (len < 0 && errno == EINTR)
			continue;
		// EOF or error
		if (len <= 0)
			break;

		bytes_left -= len;
		res.append(buff, len);
	}

	return res;
}

string getFileContents(const char* file) {
	int fd;
	while ((fd = open(file, O_RDONLY | O_LARGEFILE)) == -1 && errno == EINTR) {}

	if (fd == -1)
		return "";

	string res = getFileContents(fd);
	while (close(fd) == -1 && errno == EINTR) {}

	return res;
}

string getFileContents(const char* file, off64_t beg, off64_t end) {
	int fd;
	while ((fd = open(file, O_RDONLY | O_LARGEFILE)) == -1 && errno == EINTR) {}

	if (fd == -1)
		return "";

	string res = getFileContents(fd, beg, end);
	while (close(fd) == -1 && errno == EINTR) {}

	return res;
}

vector<string> getFileByLines(const char* file, int flags, size_t first,
		size_t last) {
	vector<string> res;

	FILE *f = fopen(file, "r");
	if (f == NULL)
		return res;

	char *buff = NULL;
	size_t n = 0, line = 0;
	ssize_t read;

	while ((read = getline(&buff, &n, f)) != -1) {
		if ((flags & GFBL_IGNORE_NEW_LINES) && buff[read - 1] == '\n')
			buff[read - 1] = '\0';

		if (line >= first && line < last)
			res.push_back(buff);

		++line;
	}

	fclose(f);
	return res;
}

int putFileContents(const char* file, const char* data, size_t len) {
	FILE *f = fopen(file, "w");
	if (f == NULL)
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
