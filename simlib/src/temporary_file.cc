#include "simlib/temporary_file.hh"
#include "simlib/file_descriptor.hh"
#include "simlib/random.hh"
#include "simlib/string_traits.hh"

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <fcntl.h>

TemporaryFile::TemporaryFile(std::string templ, mode_t mode) {
	assert(has_suffix(templ, "XXXXXX"));
	auto opt_fd = create_unique_file(AT_FDCWD, templ.data(), templ.size(), 6,
	                                 O_RDONLY | O_CLOEXEC, mode);
	if (not opt_fd) {
		THROW("create_unique_file()", errmsg());
	}
	path_ = std::move(templ);
}

std::optional<FileDescriptor>
create_unique_file(int dirfd, char* path_buff, size_t buff_len,
                   size_t suffix_len, int open_flags, mode_t mode) noexcept {
	assert(buff_len >= suffix_len);
	assert(path_buff[buff_len] == '\0');

	FileDescriptor fd;
	const size_t tries = (suffix_len == 0 ? 1 : 256);
	for (size_t try_num = 0; try_num < tries; ++try_num) {
		for (size_t i = buff_len - suffix_len; i < buff_len; ++i) {
			path_buff[i] = get_random('a', 'z');
		}

		fd = openat(dirfd, path_buff, open_flags | O_EXCL | O_CREAT, mode);
		if (fd.is_open()) {
			return fd;
		}
		if (errno == EEXIST) {
			continue;
		}
		break;
	}
	return std::nullopt; // Could not create file or openat() error
}
