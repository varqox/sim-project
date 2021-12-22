#pragma once

#include <chrono>
#include <csignal>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief Get absolute path of executable of the process with pid @p pid
 * @details executable path is always absolute, notice that if executable is
 *   unlined then path will have additional " (deleted)" suffix
 *
 * @param pid - PID of a process of which the executable path will be obtained
 *
 * @return absolute path of @p pid executable
 *
 * @errors If readlink(2) fails then std::runtime_error will be thrown
 */
std::string executable_path(pid_t pid);

/**
 * @brief Get a vector of processes pids which are instances of one of
 *   @p exec_set
 * @details Function check every accessible process if matches
 *
 * @param exec_set paths to executables (if all are absolute, then getting CWD
 *   is omitted)
 * @param include_me whether include the calling process in the result
 *   if matches or not
 *
 * @return vector of pids of matched processes
 *
 * @errors Exceptions from get_cwd() or if opendir(2) fails then
 *   std::runtime_error will be thrown
 */
std::vector<pid_t> find_processes_by_executable_path(
        std::vector<std::string> exec_set, bool include_me = false);
/**
 * @brief Kills processes that have executable files in @p exec_set
 * @details First tries with SIGTERM, but after @p wait_timeout sends SIGKILL if
 *   @p kill_after_waiting is true
 *
 * @param exec_set paths to executables (if absolute, getting CWD is omitted)
 * @param wait_timeout how long to wait for processes to die (if unset, wait
 *   indefinitely)
 * @param kill_after_waiting whether to send SIGKILL if process is still alive
 *   after wait_timeout
 */
void kill_processes_by_exec(std::vector<std::string> exec_set,
        std::optional<std::chrono::duration<double>> wait_timeout = std::nullopt,
        bool kill_after_waiting = false, int terminate_signal = SIGTERM);

enum class ArchKind : int8_t {
    i386 = 0,
    x86_64 = 1,
};

/**
 * @brief Detects architecture of running process @p pid
 * @details Currently it only detects i386 and x86_64 (see ArchKind above)
 *
 * @param pid pid of process to detect architecture
 *
 * @return detected architecture
 *
 * @errors If architecture is different from allowed or any error occurs
 *   an exception of type std::runtime_error is thrown
 */
ArchKind detect_architecture(pid_t pid);
