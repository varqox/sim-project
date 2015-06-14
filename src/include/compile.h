#pragma once

#include <string>

/**
 * @brief Compiles C++ @p source to @p exec
 * @details If compilation is not successful then errors are placed if c_errors
 * (if not NULL)
 *
 * @param source C++ source filename
 * @param exec output executable filename
 * @param verbosity verbosity level: 0 - quiet (no output), 1 - (errors only),
 * 2 or more - verbose mode
 * @param c_errors pointer to string to which compilation errors will be placed
 * @param c_errors_max_len maximum c_errors length
 * @return 0 on success, non-zero value on error
 */
int compile(const std::string& source, const std::string& exec,
	unsigned verbosity, std::string* c_errors = NULL,
	size_t c_errors_max_len = -1);
