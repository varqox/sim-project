#include "../include/filesystem.hh"
#include "../include/debug.hh"
#include "../include/logger.hh"
#include "../include/path.hh"
#include "../include/process.hh"
#include "../include/utilities.hh"

#include <dirent.h>

using std::array;
using std::string;
using std::unique_ptr;
using std::vector;

int open_unlinked_tmp_file(int flags) noexcept {
	int fd;
#ifdef O_TMPFILE
	fd = open("/tmp", O_TMPFILE | O_RDWR | O_EXCL | flags, S_0600);
	if (fd != -1)
		return fd;

	// If errno == EINVAL, then fall back to mkostemp(3)
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

TemporaryDirectory::TemporaryDirectory(FilePath templ) {
	size_t size = templ.size();
	if (size > 0) {
		// Fill name_
		while (size && templ[size - 1] == '/')
			--size;

		name_.reset(new char[size + 2]);

		memcpy(name_.get(), templ, size);
		name_.get()[size] = name_.get()[size + 1] = '\0';

		// Create directory with permissions (mode: 0700/rwx------)
		if (mkdtemp(name_.get()) == nullptr)
			THROW("Cannot create temporary directory");

		// name_ is absolute
		if (name_.get()[0] == '/')
			path_ = name();
		// name_ is not absolute
		else
			path_ = concat_tostr(get_cwd(), name());

		// Make path_ absolute
		path_ = path_absolute(path_);
		if (path_.back() != '/')
			path_ += '/';

		name_.get()[size] = '/';
	}
}

TemporaryDirectory::~TemporaryDirectory() {
#ifdef DEBUG
	if (exists() && remove_r(path_) == -1)
		errlog("Error: remove_r()", errmsg()); // We cannot throw because
		                                       // throwing from the destructor
		                                       // is (may be) UB
#else
	if (exists())
		(void)remove_r(path_); // Return value is ignored, we cannot throw
		                       // (because throwing from destructor is (may be)
		                       //   UB), logging it is also not so good
#endif
}

int mkdir_r(string path, mode_t mode) noexcept {
	if (path.size() >= PATH_MAX) {
		errno = ENAMETOOLONG;
		return -1;
	}

	// Add ending slash (if not exists)
	if (path.empty() || path.back() != '/')
		path += '/';

	size_t end = 1; // If there is a leading slash, it will be omitted
	int res = 0;
	while (end < path.size()) {
		while (path[end] != '/')
			++end;

		path[end] = '\0'; // Separate subpath
		res = mkdir(path.data(), mode);
		if (res == -1 && errno != EEXIST)
			return -1;

		path[end++] = '/';
	}

	return res;
}

/**
 * @brief Removes recursively directory @p path relative to a directory
 *   file descriptor @p dirfd
 *
 * @param dirfd directory file descriptor
 * @param path directory pathname (relative to @p dirfd)
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for fstatat64(2), openat(2), unlinkat(2),
 *   fdopendir(3)
 */
static int __remove_rat(int dirfd, FilePath path) noexcept {
	int fd =
	   openat(dirfd, path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (fd == -1)
		return unlinkat(dirfd, path, 0);

	DIR* dir = fdopendir(fd);
	if (dir == nullptr) {
		close(fd);
		return unlinkat(dirfd, path, AT_REMOVEDIR);
	}

	int ec, rc = 0;
	for_each_dir_component(
	   dir,
	   [&](dirent* file) -> bool {
#ifdef _DIRENT_HAVE_D_TYPE
		   if (file->d_type == DT_DIR || file->d_type == DT_UNKNOWN) {
#endif
			   if (__remove_rat(fd, file->d_name)) {
				   ec = errno;
				   rc = -1;
				   return false;
			   }
#ifdef _DIRENT_HAVE_D_TYPE
		   } else if (unlinkat(fd, file->d_name, 0)) {
			   ec = errno;
			   rc = -1;
			   return false;
		   }
#endif

		   return true;
	   },
	   [&] {
		   ec = errno;
		   rc = -1;
	   });

	(void)closedir(dir);

	if (rc == -1) {
		errno = ec;
		return -1;
	}

	return unlinkat(dirfd, path, AT_REMOVEDIR);
}

int remove_rat(int dirfd, FilePath path) noexcept {
	return __remove_rat(dirfd, path);
}

int remove_dir_contents_at(int dirfd, FilePath pathname) noexcept {
	int fd =
	   openat(dirfd, pathname, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (fd == -1)
		return -1;

	DIR* dir = fdopendir(fd);
	if (dir == nullptr) {
		int ec = errno;
		close(fd);
		errno = ec;
		return -1;
	}

	int ec, rc = 0;
	for_each_dir_component(
	   dir,
	   [&](dirent* file) -> bool {
#ifdef _DIRENT_HAVE_D_TYPE
		   if (file->d_type == DT_DIR || file->d_type == DT_UNKNOWN) {
#endif
			   if (__remove_rat(fd, file->d_name)) {
				   ec = errno;
				   rc = -1;
				   return false;
			   }
#ifdef _DIRENT_HAVE_D_TYPE
		   } else if (unlinkat(fd, file->d_name, 0)) {
			   ec = errno;
			   rc = -1;
			   return false;
		   }
#endif

		   return true;
	   },
	   [&] {
		   ec = errno;
		   rc = -1;
	   });

	(void)closedir(dir);

	if (rc == -1)
		errno = ec;

	return rc;
}

int create_subdirectories(FilePath file) noexcept {
	StringView path(file);
	path.remove_trailing([](char c) { return (c != '/'); });
	if (path.empty())
		return 0;

	return mkdir_r(path.to_string());
}

int blast(int infd, int outfd) noexcept {
	array<char, 65536> buff;
	ssize_t len, written;
	while (len = read(infd, buff.data(), buff.size()),
	       len > 0 || (len == -1 && errno == EINTR)) {
		ssize_t pos = 0;
		while (pos < len) {
			written = write(outfd, buff.data() + pos, len - pos);
			if (written > 0)
				pos += written;
			else if (errno != EINTR)
				return -1;
		}
	}

	if (len == -1)
		return -1;

	return 0;
}

int copyat(int dirfd1, FilePath src, int dirfd2, FilePath dest,
           mode_t mode) noexcept {
	FileDescriptor in(openat(dirfd1, src, O_RDONLY | O_CLOEXEC));
	if (in == -1)
		return -1;

	FileDescriptor out(
	   openat(dirfd2, dest, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode));
	if (out == -1)
		return -1;

	int res = blast(in, out);
	off64_t offset = lseek64(out, 0, SEEK_CUR);
	if (offset == static_cast<decltype(offset)>(-1))
		return -1;

	if (ftruncate64(out, offset))
		return -1;

	return res;
}

/**
 * @brief Copies directory @p src to @p dest relative to the directory file
 *   descriptors
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
static int __copy_rat(int dirfd1, FilePath src, int dirfd2,
                      FilePath dest) noexcept {
	int src_fd = openat(dirfd1, src, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (src_fd == -1) {
		if (errno == ENOTDIR)
			return copyat(dirfd1, src, dirfd2, dest);

		return -1;
	}

	// Do not use src permissions
	mkdirat(dirfd2, dest, S_0755);

	int dest_fd = openat(dirfd2, dest, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (dest_fd == -1) {
		close(src_fd);
		return -1;
	}

	DIR* src_dir = fdopendir(src_fd);
	if (src_dir == nullptr) {
		close(src_fd);
		close(dest_fd);
		return -1;
	}

	int ec, rc = 0;
	for_each_dir_component(
	   src_dir,
	   [&](dirent* file) -> bool {
#ifdef _DIRENT_HAVE_D_TYPE
		   if (file->d_type == DT_DIR || file->d_type == DT_UNKNOWN) {
#endif
			   if (__copy_rat(src_fd, file->d_name, dest_fd, file->d_name)) {
				   ec = errno;
				   rc = -1;
				   return false;
			   }

#ifdef _DIRENT_HAVE_D_TYPE
		   } else {
			   if (copyat(src_fd, file->d_name, dest_fd, file->d_name)) {
				   ec = errno;
				   rc = -1;
				   return false;
			   }
		   }
#endif

		   return true;
	   },
	   [&] {
		   ec = errno;
		   rc = -1;
	   });

	closedir(src_dir);
	close(dest_fd);

	if (rc == -1)
		errno = ec;

	return rc;
}

int copy_rat(int dirfd1, FilePath src, int dirfd2, FilePath dest) noexcept {
	struct stat64 sb;
	if (fstatat64(dirfd1, src, &sb, AT_SYMLINK_NOFOLLOW) == -1)
		return -1;

	if (S_ISDIR(sb.st_mode))
		return __copy_rat(dirfd1, src, dirfd2, dest);

	return copyat(dirfd1, src, dirfd2, dest);
}

int copy_r(FilePath src, FilePath dest, bool create_subdirs) noexcept {
	if (create_subdirs)
		create_subdirectories(CStringView(dest));

	return copy_rat(AT_FDCWD, src, AT_FDCWD, dest);
}

int move(FilePath oldpath, FilePath newpath, bool create_subdirs) noexcept {
	if (create_subdirs)
		create_subdirectories(CStringView(newpath));

	if (rename(oldpath, newpath) == -1) {
		if (errno == EXDEV && copy_r(oldpath, newpath, false) == 0)
			return remove_r(oldpath);

		return -1;
	}

	return 0;
}

int create_file(FilePath pathname, mode_t mode) noexcept {
	int fd = creat(pathname, mode);
	if (fd == -1)
		return -1;

	return close(fd);
}

size_t read_all(int fd, void* buf, size_t count) noexcept {
	ssize_t k;
	size_t pos = 0;
	uint8_t* buff = static_cast<uint8_t*>(buf);
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

size_t write_all(int fd, const void* buf, size_t count) noexcept {
	ssize_t k;
	size_t pos = 0;
	const uint8_t* buff = static_cast<const uint8_t*>(buf);
	errno = 0;
	do {
		k = write(fd, buff + pos, count - pos);
		if (k >= 0)
			pos += k;
		else if (errno != EINTR)
			return pos; // Error

	} while (pos < count);

	errno = 0; // No error (need to set again because errno may equal to EINTR)
	return count;
}

string get_file_contents(int fd, size_t bytes) {
	string res;
	array<char, 65536> buff;
	while (bytes > 0) {
		ssize_t len = read(fd, buff.data(), std::min(buff.size(), bytes));
		// Interrupted by signal
		if (len < 0 && errno == EINTR)
			continue;
		// Error
		if (len < 0)
			THROW("read() failed", errmsg());
		// EOF
		if (len == 0)
			break;

		bytes -= len;
		res.append(buff.data(), len);
	}

	return res;
}

string get_file_contents(int fd, off64_t beg, off64_t end) {
	array<char, 65536> buff;
	off64_t size = lseek64(fd, 0, SEEK_END);
	if (beg < 0)
		beg = std::max<off64_t>(size + beg, 0);
	if (size == (off64_t)-1)
		THROW("lseek64() failed", errmsg());
	if (beg > size)
		return "";

	// Change end to the valid value
	if (size < end || end < 0)
		end = size;

	if (end < beg)
		end = beg;
	// Reposition to beg
	if (beg != lseek64(fd, beg, SEEK_SET))
		THROW("lseek64() failed", errmsg());

	off64_t bytes_left = end - beg;
	string res;
	while (bytes_left > 0) {
		ssize_t len =
		   read(fd, buff.data(), std::min<off64_t>(buff.size(), bytes_left));
		// Interrupted by signal
		if (len < 0 && errno == EINTR)
			continue;
		// Error
		if (len < 0)
			THROW("read() failed", errmsg());
		// EOF
		if (len == 0)
			break;

		bytes_left -= len;
		res.append(buff.data(), len);
	}

	return res;
}

string get_file_contents(FilePath file) {
	FileDescriptor fd;
	while (fd.open(file, O_RDONLY | O_CLOEXEC) == -1 && errno == EINTR) {
	}

	if (fd == -1)
		THROW("Failed to open file `", file, '`', errmsg());

	return get_file_contents(fd);
}

string get_file_contents(FilePath file, off64_t beg, off64_t end) {
	FileDescriptor fd;
	while (fd.open(file, O_RDONLY | O_CLOEXEC) == -1 && errno == EINTR) {
	}

	if (fd == -1)
		THROW("Failed to open file `", file, '`', errmsg());

	return get_file_contents(fd, beg, end);
}

void put_file_contents(FilePath file, const char* data, size_t len,
                       mode_t mode) {
	FileDescriptor fd {file, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode};
	if (fd == -1)
		THROW("open() failed", errmsg());

	if (len == size_t(-1))
		len = std::char_traits<char>::length(data);

	if (write_all(fd, data, len) != len)
		THROW("write() failed", errmsg());
}

string humanize_file_size(uint64_t size) {
	constexpr uint64_t MIN_KIB = 1ull << 10;
	constexpr uint64_t MIN_MIB = 1ull << 20;
	constexpr uint64_t MIN_GIB = 1ull << 30;
	constexpr uint64_t MIN_TIB = 1ull << 40;
	constexpr uint64_t MIN_PIB = 1ull << 50;
	constexpr uint64_t MIN_EIB = 1ull << 60;
	constexpr uint64_t MIN_3DIGIT_KIB = 102349ull;
	constexpr uint64_t MIN_3DIGIT_MIB = 104805172ull;
	constexpr uint64_t MIN_3DIGIT_GIB = 107320495309ull;
	constexpr uint64_t MIN_3DIGIT_TIB = 109896187196212ull;
	constexpr uint64_t MIN_3DIGIT_PIB = 112533595688920269ull;

	// Bytes
	if (size < MIN_KIB)
		return (size == 1 ? "1 byte" : concat_tostr(size, " bytes"));

	double dsize = size;
	// KiB
	if (size < MIN_3DIGIT_KIB)
		return to_string(dsize / MIN_KIB, 1) + " KiB";
	if (size < MIN_MIB)
		return to_string(dsize / MIN_KIB, 0) + " KiB";
	// MiB
	if (size < MIN_3DIGIT_MIB)
		return to_string(dsize / MIN_MIB, 1) + " MiB";
	if (size < MIN_GIB)
		return to_string(dsize / MIN_MIB, 0) + " MiB";
	// GiB
	if (size < MIN_3DIGIT_GIB)
		return to_string(dsize / MIN_GIB, 1) + " GiB";
	if (size < MIN_TIB)
		return to_string(dsize / MIN_GIB, 0) + " GiB";
	// TiB
	if (size < MIN_3DIGIT_TIB)
		return to_string(dsize / MIN_TIB, 1) + " TiB";
	if (size < MIN_PIB)
		return to_string(dsize / MIN_TIB, 0) + " TiB";
	// PiB
	if (size < MIN_3DIGIT_PIB)
		return to_string(dsize / MIN_PIB, 1) + " PiB";
	if (size < MIN_EIB)
		return to_string(dsize / MIN_PIB, 0) + " PiB";
	// EiB
	return to_string(dsize / MIN_EIB, 1) + " EiB";
}
