#include "simlib/file_manip.hh"
#include "simlib/call_in_destructor.hh"
#include "simlib/directory.hh"
#include "simlib/file_descriptor.hh"
#include "simlib/random.hh"

using std::array;
using std::string;

int mkdir_r(string path, mode_t mode) noexcept {
	if (path.size() >= PATH_MAX) {
		errno = ENAMETOOLONG;
		return -1;
	}

	// Add ending slash (if not exists)
	if (path.empty() || path.back() != '/')
		path += '/';

	size_t end = 1; // If there is a leading slash, it will be omitted
	while (end < path.size()) {
		while (path[end] != '/')
			++end;

		path[end] = '\0'; // Separate subpath
		if (mkdir(path.data(), mode) == -1 && errno != EEXIST)
			return -1;

		path[end++] = '/';
	}

	return 0;
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

int copyat(int src_dirfd, FilePath src, int dest_dirfd, FilePath dest,
           mode_t mode) noexcept {
	FileDescriptor in(openat(src_dirfd, src, O_RDONLY | O_CLOEXEC));
	if (in == -1)
		return -1;

	FileDescriptor out(openat(dest_dirfd, dest,
	                          O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode));
	if (not out.is_open()) {
		if (errno != ETXTBSY)
			return -1;

		// Try to construct temporary file and rename it to the destination file
		try {
			InplaceBuff<PATH_MAX> tmp_dest(std::in_place, dest, '.');
			const auto tmp_dest_orig_size = tmp_dest.size;
			constexpr uint SUFFIX_LEN = 10;
			for (size_t iter = 0; iter < 128; ++iter) {
				tmp_dest.size = tmp_dest_orig_size;
				for (size_t i = 0; i < SUFFIX_LEN; ++i)
					tmp_dest.append(get_random('a', 'z'));

				out = openat(dest_dirfd, tmp_dest.to_cstr().data(),
				             O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, mode);
				if (not out.is_open()) {
					if (errno == EEXIST)
						continue;
					return -1; // Other error occurred
				}

				CallInDtor tmp_file_remover = [&] {
					(void)unlinkat(dest_dirfd, tmp_dest.to_cstr().data(), 0);
				};
				if (renameat(dest_dirfd, tmp_dest.to_cstr().data(), dest_dirfd,
				             dest))
					return -1;

				// Success
				tmp_file_remover.cancel();
				break;
			}
		} catch (const std::bad_alloc&) {
			errno = ENOMEM;
			return -1;
		} catch (...) {
			errno = ETXTBSY; // Revert to the previous error
			return -1;
		}

		if (not out.is_open())
			return -1;
	}

	int res = blast(in, out);
	off64_t offset = lseek64(out, 0, SEEK_CUR);
	if (offset == static_cast<decltype(offset)>(-1))
		return -1;

	if (ftruncate64(out, offset))
		return -1;

	return res;
}

int copyat(int src_dirfd, FilePath src, int dest_dirfd,
           FilePath dest) noexcept {
	struct stat64 sb;
	if (fstatat64(src_dirfd, src, &sb, 0))
		return -1;

	return copyat(src_dirfd, src, dest_dirfd, dest, sb.st_mode & ACCESSPERMS);
}

/**
 * @brief Copies directory @p src to @p dest relative to the directory file
 *   descriptors
 *
 * @param src_dirfd directory file descriptor
 * @param src source directory (relative to @p src_dirfd)
 * @param dest_dirfd directory file descriptor
 * @param dest destination directory (relative to @p dest_dirfd)
 *
 * @return 0 on success, -1 on error
 *
 * @errors The same that occur for fstat64(2), openat(2), fdopendir(3),
 *   mkdirat(2), copyat()
 */
static int __copy_rat(int src_dirfd, FilePath src, int dest_dirfd,
                      FilePath dest) noexcept {
	int src_fd = openat(src_dirfd, src, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (src_fd == -1) {
		if (errno == ENOTDIR)
			return copyat(src_dirfd, src, dest_dirfd, dest);

		return -1;
	}

	// Do not use src permissions
	mkdirat(dest_dirfd, dest, S_0755);

	int dest_fd = openat(dest_dirfd, dest, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
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

int copy_rat(int src_dirfd, FilePath src, int dest_dirfd,
             FilePath dest) noexcept {
	struct stat64 sb;
	if (fstatat64(src_dirfd, src, &sb, 0) == -1)
		return -1;

	if (S_ISDIR(sb.st_mode))
		return __copy_rat(src_dirfd, src, dest_dirfd, dest);

	return copyat(src_dirfd, src, dest_dirfd, dest, sb.st_mode & ACCESSPERMS);
}

int copy_r(FilePath src, FilePath dest, bool create_subdirs) noexcept {
	if (create_subdirs and create_subdirectories(CStringView(dest)) == -1)
		return -1;

	return copy_rat(AT_FDCWD, src, AT_FDCWD, dest);
}

int move(FilePath oldpath, FilePath newpath, bool create_subdirs) noexcept {
	if (create_subdirs and create_subdirectories(CStringView(newpath)) == -1)
		return -1;

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
