#pragma once

#include "../string.h"

#include <chrono>
#include <optional>
#include <vector>

namespace sim {

/**
 * @brief Runs compiler via PRoot (or not)
 * @details If compilation is not successful then errors are placed in
 *   @p c_errors (if c_errors is not NULL)
 *
 * @param dir_to_chdir path of the directory to chdir(2) before running the
 *   compiler (the CWD remains unchanged because the change only affect the
 *   invoked compiler)
 * @param compile_command compilation command to run
 * @param time_limit time limit for compiler
 * @param c_errors pointer to string in which compilation errors will be placed
 * @param c_errors_max_len maximum c_errors length
 * @param proot_path path to PRoot executable (to pass to spawn()), if empty
 *   then the compiler will be run WITHOUT PRoot
 *
 * @return 0 on success, non-zero value on error
 */
int compile(StringView dir_to_chdir, std::vector<std::string> compile_command,
            std::optional<std::chrono::nanoseconds> time_limit,
            std::string* c_errors, size_t c_errors_max_len,
            const std::string& proot_path);

} // namespace sim
