#pragma once

#include <fcntl.h>
#include <simlib/file_descriptor.hh>

/**
 * @brief Creates (and opens) unlinked temporary file
 * @details Uses open(2) if O_TMPFILE is defined, or mkostemp(3)
 *
 * @param flags flags which be ORed with O_TMPFILE | O_RDWR in open(2) or
 *   passed to mkostemp(3)
 *
 * @return file descriptor on success, -1 on error
 *
 * @errors The same that occur for open(2) (if O_TMPFILE is defined) or
 *   mkostemp(3)
 */
FileDescriptor open_unlinked_tmp_file(int flags = O_CLOEXEC) noexcept;
