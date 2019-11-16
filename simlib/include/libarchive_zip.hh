#pragma once

#include "debug.hh"
#include "defer.hh"
#include "directory.hh"
#include "file_descriptor.hh"
#include "path.hh"

#if __has_include(<archive.h>) and __has_include(<archive_entry.h>)

#include <archive.h>
#include <archive_entry.h>
#include <climits>

/**
 * @brief Runs @p entry_callback on every archive @p filename's entry
 *
 * @param fd file descriptor of the archive
 * @param setup_archive function to run before reading the archive, should take
 *   archive* as the argument, most useful to set the accepted archive formats
 * @param entry_callback function to call on every entry, should take one
 *   argument - archive_entry*, if it return sth convertible to
 *   false the lookup will break
 */
template <class Func, class UnaryFunc>
void skim_archive(int fd, Func&& setup_archive, UnaryFunc&& entry_callback) {
	struct archive* in = archive_read_new();
	throw_assert(in);
	Defer in_guard([&] { archive_read_free(in); });

	setup_archive(in);

	if (archive_read_open_fd(in, fd, 1 << 18))
		THROW("archive_read_open_fd() - ", archive_error_string(in));

	// Read contents
	for (;;) {
		struct archive_entry* entry;
		int r = archive_read_next_header(in, &entry);
		if (r == ARCHIVE_EOF)
			break;
		if (r)
			THROW("archive_read_next_header() - ", archive_error_string(in));

		if constexpr (std::is_convertible_v<decltype(entry_callback(entry)),
		                                    bool>) {
			if (not entry_callback(entry))
				break;
		} else {
			entry_callback(entry);
		}
	}
}

/**
 * @brief Runs @p entry_callback on every archive @p filename's entry
 *
 * @param filename path of the archive
 * @param setup_archive function to run before reading the archive, should take
 *   archive* as the argument, most useful to set the accepted archive formats
 * @param entry_callback function to call on every entry, should take one
 *   argument - archive_entry*
 */
template <class Func, class UnaryFunc>
void skim_archive(FilePath filename, Func&& setup_archive,
                  UnaryFunc&& entry_callback) {
	FileDescriptor fd {filename, O_RDONLY | O_CLOEXEC};
	if (fd == -1)
		THROW("Failed to open file `", filename, '`', errmsg());

	return skim_archive(fd, std::forward<Func>(setup_archive),
	                    std::forward<UnaryFunc>(entry_callback));
}

/**
 * @brief Runs @p entry_callback on every archive @p filename's entry
 *
 * @param fd file descriptor of the archive
 * @param entry_callback function to call on every entry, should take one
 *   argument - archive_entry*
 */
template <class UnaryFunc>
void skim_zip(int fd, UnaryFunc&& entry_callback) {
	return skim_archive(fd, archive_read_support_format_zip,
	                    std::forward<UnaryFunc>(entry_callback));
}

/**
 * @brief Runs @p entry_callback on every archive @p filename's entry
 *
 * @param filename path of the zip archive
 * @param entry_callback function to call on every entry, should take one
 *   argument - archive_entry*
 */
template <class UnaryFunc>
void skim_zip(FilePath filename, UnaryFunc&& entry_callback) {
	return skim_archive(filename, archive_read_support_format_zip,
	                    std::forward<UnaryFunc>(entry_callback));
}

/**
 * @brief Extracts archive @p filename to the current working directory
 *
 * @param archive_fd archive file descriptor
 * @param flags flags to pass to archive_write_disk_set_options()
 * @param setup_archives function to run before reading the archive, should
 *   take arguments: archive* in, archive* out - input and output archive
 *   handlers, most useful to set the accepted archive formats
 * @param extract_entry function to call on every entry, should take one
 *   argument - archive_entry* and return bool determining whether or not
 *   extract the entry
 * @param dest_dir path to the directory to which files will be extracted
 */
template <class Func, class UnaryFunc>
void extract(int archive_fd, int flags, Func&& setup_archives,
             UnaryFunc&& extract_entry, StringView dest_dir = ".") {
	dest_dir.remove_trailing('/');
	// Do not trust extracted archives
	flags |= ARCHIVE_EXTRACT_SECURE_SYMLINKS;

	/* Since the path is evaluated safely with abspath() below flags are
	    disabled as they may cause problems with the @p dest_dir */
	// flags |= ARCHIVE_EXTRACT_SECURE_NODOTDOT;
	// flags |= ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS;

	struct archive* in = archive_read_new();
	throw_assert(in);
	Defer in_guard([&] { archive_read_free(in); });

	struct archive* out = archive_write_disk_new();
	throw_assert(out);
	Defer out_guard([&] { archive_write_free(out); });

	archive_write_disk_set_options(out, flags);
	setup_archives(in, out);

	if (archive_read_open_fd(in, archive_fd, 1 << 18))
		THROW("archive_read_open_fd() - ", archive_error_string(in));

	// Read contents
	InplaceBuff<PATH_MAX> dest_path;
	dest_path.append(dest_dir);
	for (;;) {
		struct archive_entry* entry;
		int r = archive_read_next_header(in, &entry);
		if (r == ARCHIVE_EOF)
			break;
		if (r)
			THROW("archive_read_next_header() - ", archive_error_string(in));

		if (!extract_entry(entry))
			continue;

		// Alter entry's pathname so that it will be placed in dest_dir
		dest_path.size = dest_dir.size();
		dest_path.append(path_absolute(archive_entry_pathname(entry)));
		archive_entry_set_pathname(entry, dest_path.to_cstr().c_str());

		if (archive_write_header(out, entry))
			THROW("archive_write_header() - ", archive_error_string(out));

		if (archive_entry_size(entry) > 0) {
			constexpr size_t BUFF_SIZE = 1 << 18;
			char buff[BUFF_SIZE];
			decltype(archive_read_data(in, buff, BUFF_SIZE)) rr;
			while ((rr = archive_read_data(in, buff, BUFF_SIZE)) > 0) {
				if ((rr = archive_write_data(out, buff, rr)) < 0)
					THROW("archive_write_data() - ", archive_error_string(out));
			}

			if (rr)
				THROW("archive_read_data() - ", archive_error_string(in));
		}

		if (archive_write_finish_entry(out))
			THROW("archive_write_finish_entry() - ", archive_error_string(out));
	}
}

/**
 * @brief Extracts archive @p filename to the current working directory
 *
 * @param filename path of the archive
 * @param flags flags to pass to archive_write_disk_set_options()
 * @param setup_archives function to run before reading the archive, should
 *   take arguments: archive* in, archive* out - input and output archive
 *   handlers, most useful to set the accepted archive formats
 * @param extract_entry function to call on every entry, should take one
 *   argument - archive_entry* and return bool determining whether or not
 *   extract the entry
 */
template <class Func, class UnaryFunc>
void extract(FilePath filename, int flags, Func&& setup_archives,
             UnaryFunc&& extract_entry, StringView dest_dir = ".") {
	FileDescriptor fd {filename, O_RDONLY | O_CLOEXEC};
	if (fd == -1)
		THROW("Failed to open file `", filename, '`', errmsg());

	return extract(fd, flags, std::forward<Func>(setup_archives),
	               std::forward<UnaryFunc>(extract_entry), dest_dir);
}

/**
 * @brief Extract @p pathname from the archive @p archive_file
 *
 * @param archive_fd archive file descriptor
 * @param setup_archive function to run before reading the archive, should take
 *   archive* as the argument, most useful to set the accepted archive formats
 * @param pathname path of the file (the one in archive) to extracted
 */
template <class Func>
std::string extract_file(int archive_fd, Func&& setup_archive,
                         StringView pathname) {
	struct archive* in = archive_read_new();
	throw_assert(in);
	Defer in_guard([&] { archive_read_free(in); });

	setup_archive(in);

	if (archive_read_open_fd(in, archive_fd, 1 << 18))
		THROW("archive_read_open_fd() - ", archive_error_string(in));

	// Read contents
	for (;;) {
		struct archive_entry* entry;
		int r = archive_read_next_header(in, &entry);
		if (r == ARCHIVE_EOF)
			break;
		if (r)
			THROW("archive_read_next_header() - ", archive_error_string(in));

		if (archive_entry_pathname(entry) != pathname)
			continue;

		std::string res(archive_entry_size(entry), '\0');
		if (res.size()) {
			auto ptr = res.data();
			auto left = res.size();
			while (left != 0) {
				auto rr = archive_read_data(in, ptr, left);
				// Yes, I know that rr == 0 is not an error but is should not
				//  happen and not checking for it may create an infinity loop
				if (rr <= 0)
					THROW("archive_read_data() - ", archive_error_string(in));

				ptr += rr;
				left -= rr;
			}
		}

		return res;
	}

	return "";
}

/**
 * @brief Extract @p pathname from the archive @p archive_file
 *
 * @param archive_file path of the archive
 * @param setup_archive function to run before reading the archive, should take
 *   archive* as the argument, most useful to set the accepted archive formats
 * @param pathname path of the file (the one in archive) to extracted
 */
template <class Func>
std::string extract_file(FilePath archive_file, Func&& setup_archive,
                         StringView pathname) {
	FileDescriptor fd {archive_file, O_RDONLY | O_CLOEXEC};
	if (fd == -1)
		THROW("Failed to open file `", archive_file, '`', errmsg());

	return extract_file(fd, std::forward<Func>(setup_archive), pathname);
}

/// Extract @p pathname from the zip archive @p zip_fd
inline std::string extract_file_from_zip(int zip_fd, StringView pathname) {
	return extract_file(zip_fd, archive_read_support_format_zip, pathname);
}

/// Extract @p pathname from the zip archive @p zip_file
inline std::string extract_file_from_zip(FilePath zip_file,
                                         StringView pathname) {
	return extract_file(zip_file, archive_read_support_format_zip, pathname);
}

// Extracts zip, for details see extract() documentation
template <class UnaryFunc>
inline void extract_zip(int zip_fd, int flags, UnaryFunc&& extract_entry,
                        StringView dest_dir = ".") {
	return extract(
	   zip_fd, flags,
	   [](archive* in, archive*) { archive_read_support_format_zip(in); },
	   std::forward<UnaryFunc>(extract_entry), dest_dir);
}

/// Extracts zip, for details see extract() documentation
inline void extract_zip(int zip_fd, int flags = ARCHIVE_EXTRACT_TIME,
                        StringView dest_dir = ".") {
	return extract_zip(
	   zip_fd, flags, [](archive_entry*) { return true; }, dest_dir);
}

// Extracts zip, for details see extract() documentation
template <class UnaryFunc>
inline void extract_zip(FilePath filename, int flags, UnaryFunc&& extract_entry,
                        StringView dest_dir = ".") {
	return extract(
	   filename, flags,
	   [](archive* in, archive*) { archive_read_support_format_zip(in); },
	   std::forward<UnaryFunc>(extract_entry), dest_dir);
}

/// Extracts zip, for details see extract() documentation
inline void extract_zip(FilePath filename, int flags = ARCHIVE_EXTRACT_TIME,
                        StringView dest_dir = ".") {
	return extract_zip(
	   filename, flags, [](archive_entry*) { return true; }, dest_dir);
}

/**
 * @brief Compresses @p filenames recursively into @p archive_filename
 *
 * @param filenames paths of files/directories to archive
 * @param archive_filename output archive's path
 * @param setup_archive function to run before reading the archive, should take
 *   archive* as the argument, most useful to set the archive format
 */
template <class Container, class Func>
void compress(Container&& filenames, FilePath archive_filename,
              Func&& setup_archive) {
	struct archive* out = archive_write_new();
	throw_assert(out);
	Defer out_guard([&] { archive_write_free(out); });

	setup_archive(out);

	FileDescriptor fd(archive_filename,
	                  O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC);
	if (fd == -1)
		THROW("Failed to open file `", archive_filename, '`', errmsg());

	if (archive_write_open_fd(out, fd))
		THROW("archive_write_open_fd() - ", archive_error_string(out));

	auto write_file = [&](FilePath pathname, struct stat64& st) {
		archive_entry* entry = archive_entry_new();
		throw_assert(entry);
		Defer entry_guard([&] { archive_entry_free(entry); });

		archive_entry_set_mode(entry, st.st_mode);
		archive_entry_set_filetype(entry, AE_IFREG);
		archive_entry_set_atime(entry, st.st_atim.tv_sec, st.st_atim.tv_nsec);
		archive_entry_set_ctime(entry, st.st_ctim.tv_sec, st.st_ctim.tv_nsec);
		archive_entry_set_mtime(entry, st.st_mtim.tv_sec, st.st_mtim.tv_nsec);
		archive_entry_set_pathname(entry, pathname);
		archive_entry_set_size(entry, st.st_size);

		if (archive_write_header(out, entry))
			THROW("archive_write_header() - ", archive_error_string(out));

		FileDescriptor f {pathname, O_RDONLY | O_CLOEXEC};
		throw_assert(f != -1);

		constexpr size_t BUFF_SIZE = 1 << 18;
		char buff[BUFF_SIZE];
		ssize_t r;
		while ((r = read(f, buff, BUFF_SIZE)) > 0) {
			if (archive_write_data(out, buff, r) != r)
				THROW("archive_write_data() - ", archive_error_string(out));
		}

		if (r)
			THROW("read()", errmsg());
	};

	InplaceBuff<PATH_MAX> pathname;
	auto impl = [&](auto&& self, struct stat64& st) -> void {
		if (!S_ISDIR(st.st_mode))
			THROW("Unsupported file type");

		// Directory
		{
			archive_entry* entry = archive_entry_new();
			throw_assert(entry);
			Defer entry_guard([&] { archive_entry_free(entry); });

			pathname.append('/');

			archive_entry_set_mode(entry, st.st_mode);
			archive_entry_set_filetype(entry, AE_IFDIR);
			archive_entry_set_atime(entry, st.st_atim.tv_sec,
			                        st.st_atim.tv_nsec);
			archive_entry_set_ctime(entry, st.st_ctim.tv_sec,
			                        st.st_ctim.tv_nsec);
			archive_entry_set_mtime(entry, st.st_mtim.tv_sec,
			                        st.st_mtim.tv_nsec);
			archive_entry_set_pathname(entry, pathname.to_cstr().c_str());
			archive_entry_set_size(entry, st.st_size);

			if (archive_write_header(out, entry))
				THROW("archive_write_header() - ", archive_error_string(out));
		}

		Directory dir(pathname);
		if (not dir)
			THROW("opendir(`", pathname, "`)", errmsg());

		for_each_dir_component(dir, [&](dirent* file) {
			size_t before_size = pathname.size;
			pathname.append(file->d_name);

			if (stat64(FilePath(pathname), &st))
				THROW("stat(`", pathname, "`)", errmsg());

			if (S_ISREG(st.st_mode))
				write_file(pathname, st);
			else
				self(self, st);

			pathname.size = before_size;
		});
	};

	for (auto&& filename : filenames) {
		struct stat64 st;
		if (stat64(filename, &st))
			THROW("stat(`", filename, "`)", errmsg());

		pathname = filename;
		throw_assert(pathname.size > 0);

		if (S_ISREG(st.st_mode)) {
			write_file(pathname, st);
		} else {
			// Remove trailing '/', but leave "/" if the pathname looks like
			// this: "/////"
			while (pathname.size > 1 and pathname.back() == '/')
				--pathname.size;
			impl(impl, st);
		}
	}

	// It may be omitted because archive_write_free() calls it automatically,
	// but as it may fail it is better to detect such cases and this check
	// cannot be safely placed in the out_guard because it may throw and
	// out_guard is executed from a destructor
	if (archive_write_close(out))
		THROW("archive_write_close() - ", archive_error_string(out));
}

/// Specialization of compress()
template <class T, class Func>
inline void compress(std::initializer_list<T> filenames,
                     FilePath archive_filename, Func&& setup_archive) {
	return compress<std::initializer_list<T>>(
	   std::move(filenames), archive_filename,
	   std::forward<Func>(setup_archive));
}

/**
 * @brief Compresses @p filenames recursively into @p zip_archive_filename
 *
 * @param filenames paths of files/directories to archive
 * @param zip_archive_filename output archive's path
 */
template <class Container>
void compress_into_zip(Container&& filenames, FilePath zip_archive_filename) {
	return compress(std::forward<Container>(filenames), zip_archive_filename,
	                archive_write_set_format_zip);
}

/// Specialization of compress_into_zip()
template <class T>
inline void compress_into_zip(std::initializer_list<T> filenames,
                              FilePath zip_archive_filename) {
	return compress_into_zip<std::initializer_list<T>>(std::move(filenames),
	                                                   zip_archive_filename);
}

#endif // __has_include(<archive.h>) and __has_include(<archive_entry.h>)

/// Places file @p filename (with a replaced path to @p new_filename) into zip
/// file @p zip_filename. If @p new_filename is empty, then @p filename is
/// used. This function may be used to add directories to the archive as well.
void update_add_file_to_zip(FilePath filename, StringView new_filename,
                            FilePath zip_filename);

/// Places data @p data (within a file @p new_filename) into zip
/// file @p zip_filename. If new_filename ends with '/' data is ignored and
/// only directory @p new_filename is created.
void update_add_data_to_zip(StringView data, StringView new_filename,
                            FilePath zip_filename);
