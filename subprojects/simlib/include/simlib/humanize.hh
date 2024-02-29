#pragma once

#include <cstdint>
#include <string>

/**
 * @brief Converts @p size, so that it human readable
 * @details It adds proper suffixes, for example:
 *   1 -> "1 byte"
 *   1023 -> "1023 bytes"
 *   1024 -> "1.0 KiB"
 *   129747 -> "127 KiB"
 *   97379112 -> "92.9 MiB"
 *
 * @param size size to humanize
 *
 * @return humanized file size
 */
std::string humanize_file_size(uint64_t size);
