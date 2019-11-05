#pragma once

#include "../sandbox.hh"

namespace sim {

/**
 * @brief Obtains checker output (truncated if too long)
 *
 * @param fd file descriptor of file to which checker wrote via stdout. (The
 *   file offset is not changed.)
 * @param max_length maximum length of the returned string
 *
 * @return checker output, truncated if too long
 *
 * @errors Throws an exception of type std::runtime_error with appropriate
 *   message
 */
std::string obtain_checker_output(int fd, size_t max_length);

} // namespace sim
