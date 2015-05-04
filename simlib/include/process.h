#pragma once

#include <string>
#include <vector>

/**
 * @brief Runs exec using execvp() with arguments @p args
 *
 * @param exec file to execute
 * @param args arguments
 *
 * @return exit code on success, -1 on error
 */
int spawn(const std::string& exec, const std::vector<std::string>& args);

/**
 * @brief Runs exec using execvp() with arguments @p args
 *
 * @param exec file to execute
 * @param argc number of arguments
 * @param args arguments
 *
 * @return exit code on success, -1 on error
 */
int spawn(const std::string& exec, size_t argc, std::string *args);
