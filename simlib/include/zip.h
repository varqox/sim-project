#pragma once

#include "debug.h"
#include "string.h"
#include "utilities.h"
#include "filesystem.h"

#if __has_include(<archive.h>) and __has_include(<archive_entry.h>)

#include <archive.h>
#include <archive_entry.h>

template<class Func, class UnaryFunc>
void skim_archive(CStringView filename, Func&& setup_archive,
	UnaryFunc&& entry_callback)
{
	struct archive* in = archive_read_new();
	throw_assert(in);
	auto in_guard = make_call_in_destructor([&] { archive_read_free(in); });

	setup_archive(in);

	FileDescriptor fd {filename, O_RDONLY | O_LARGEFILE};
	if (fd == -1)
		THROW("Failed to open file `", filename, '`', error(errno));

	if (archive_read_open_fd(in, fd, 1 << 18))
		THROW("archive_read_open_fd() - ", archive_error_string(in));

	// Read contents
	for (;;) {
		struct archive_entry *entry;
		int r = archive_read_next_header(in, &entry);
		if (r == ARCHIVE_EOF)
			break;
		if (r)
			THROW("archive_read_next_header() - ", archive_error_string(in));

		entry_callback(entry);
	}
}

template<class Func, class UnaryFunc>
void extract(CStringView filename, int flags,
	Func&& setup_archives, UnaryFunc&& extract_entry)
{
	// Do not trust extracted archives
	flags |= ARCHIVE_EXTRACT_SECURE_SYMLINKS;
	flags |= ARCHIVE_EXTRACT_SECURE_NODOTDOT;
	flags |= ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS;

	struct archive* in = archive_read_new();
	throw_assert(in);
	auto in_guard = make_call_in_destructor([&] { archive_read_free(in); });

	struct archive* out = archive_write_disk_new();
	throw_assert(out);
	auto out_guard = make_call_in_destructor([&] { archive_write_free(out); });

	archive_write_disk_set_options(out, flags);
	setup_archives(in, out);

	FileDescriptor fd {filename, O_RDONLY | O_LARGEFILE};
	if (fd == -1)
		THROW("Failed to open file `", filename, '`', error(errno));

	if (archive_read_open_fd(in, fd, 1 << 18))
		THROW("archive_read_open_fd() - ", archive_error_string(in));

	// Read contents
	for (;;) {
		struct archive_entry *entry;
		int r = archive_read_next_header(in, &entry);
		if (r == ARCHIVE_EOF)
			break;
		if (r)
			THROW("archive_read_next_header() - ", archive_error_string(in));

		if (!extract_entry(entry))
			continue;

		if (archive_write_header(out, entry))
			THROW("archive_write_header() - ", archive_error_string(out));

		if (archive_entry_size(entry) > 0) {
			constexpr size_t BUFF_SIZE = 1 << 18;
			char buff[BUFF_SIZE];
			la_ssize_t rr;
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

template<class UnaryFunc>
inline void extract_zip(CStringView filename, int flags,
	UnaryFunc&& extract_entry)
{
	return extract(filename, flags, [](archive* in, archive*){
		archive_read_support_format_zip(in);
	}, std::forward<UnaryFunc>(extract_entry));
}

inline void extract_zip(CStringView filename, int flags = ARCHIVE_EXTRACT_TIME)
{
	return extract_zip(filename, flags, [](archive_entry*) { return true; });
}

template<class Container, class Func>
void compress(Container&& filenames, CStringView archive_filename,
	Func&& setup_archive)
{
	FileDescriptor fd;
	struct archive* out = archive_write_new();
	throw_assert(out);
	auto out_guard = make_call_in_destructor([&] { archive_write_free(out); });

	setup_archive(out);

	fd.open(archive_filename, O_WRONLY | O_CREAT | O_TRUNC);
	if (fd == -1)
		THROW("Failed to open file `", archive_filename, '`', error(errno));

	if (archive_write_open_fd(out, fd))
		THROW("archive_write_open_fd() - ", archive_error_string(out));

	auto write_file = [&] (CStringView pathname, struct stat64& st) {
		archive_entry* entry = archive_entry_new();
		throw_assert(entry);

		archive_entry_set_mode(entry, st.st_mode);
		archive_entry_set_filetype(entry, AE_IFREG);
		archive_entry_set_atime(entry, st.st_atim.tv_sec, st.st_atim.tv_nsec);
		archive_entry_set_ctime(entry, st.st_ctim.tv_sec, st.st_ctim.tv_nsec);
		archive_entry_set_mtime(entry, st.st_mtim.tv_sec, st.st_mtim.tv_nsec);
		archive_entry_set_pathname(entry, pathname.c_str());
		archive_entry_set_size(entry, st.st_size);

		if (archive_write_header(out, entry))
			THROW("archive_write_header() - ", archive_error_string(out));

		FileDescriptor f {pathname, O_RDONLY | O_LARGEFILE};
		throw_assert(f != -1);

		constexpr size_t BUFF_SIZE = 1 << 18;
		char buff[BUFF_SIZE];
		ssize_t r;
		while ((r = read(f, buff, BUFF_SIZE)) > 0) {
			if (archive_write_data(out, buff, r) != r)
				THROW("archive_write_data() - ", archive_error_string(out));
		}

		if (r)
			THROW("read()", error(errno));

		archive_entry_free(entry);
	};

	InplaceBuff<PATH_MAX> pathname;
	std::function<void(struct stat64&)> impl = [&] (struct stat64& st) {
		if (!S_ISDIR(st.st_mode))
			THROW("Unsupported file type");

		// Directory
		{
			archive_entry* entry = archive_entry_new();
			throw_assert(entry);

			pathname.append('/', '\0');
			--pathname.size;

			archive_entry_set_mode(entry, st.st_mode);
			archive_entry_set_filetype(entry, AE_IFDIR);
			archive_entry_set_atime(entry, st.st_atim.tv_sec, st.st_atim.tv_nsec);
			archive_entry_set_ctime(entry, st.st_ctim.tv_sec, st.st_ctim.tv_nsec);
			archive_entry_set_mtime(entry, st.st_mtim.tv_sec, st.st_mtim.tv_nsec);
			archive_entry_set_pathname(entry, pathname.data());
			archive_entry_set_size(entry, st.st_size);

			if (archive_write_header(out, entry))
				THROW("archive_write_header() - ", archive_error_string(out));

			archive_entry_free(entry);
		}

		DIR* dir = opendir(pathname.data());
		if (not dir)
			THROW("opendir(`", pathname, "`)", strerror(errno));

		auto dir_guard = make_call_in_destructor([&] { closedir(dir); });

		dirent* file;
		while ((file = readdir(dir)))
			if (strcmp(file->d_name, ".") && strcmp(file->d_name, "..")) {
				size_t before_size = pathname.size;
				pathname.append(file->d_name, '\0');
				--pathname.size;

				if (stat64(pathname.data(), &st))
					THROW("stat(`", pathname, "`)", error(errno));

				if (S_ISREG(st.st_mode))
					write_file(CStringView{pathname.data(), pathname.size}, st);
				else
					impl(st);

				pathname.size = before_size;
			}
	};


	for (auto&& filename : filenames) {
		struct stat64 st;
		if (stat64(filename, &st))
			THROW("stat(`", filename, "`)", error(errno));

		pathname = filename;

		if (S_ISREG(st.st_mode)) {
			pathname.append('\0');
			--pathname.size;
			write_file(CStringView{pathname.data(), pathname.size}, st);
		} else
			impl(st);
	}
}

template<class T, class Func>
inline void compress(std::initializer_list<T> filenames,
	CStringView archive_filename, Func&& setup_archive)
{
	return compress<std::initializer_list<T>>(std::move(filenames),
		archive_filename, std::forward<Func>(setup_archive));
}

#endif // __has_include(<archive.h>) and __has_include(<archive_entry.h>)
