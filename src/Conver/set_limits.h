#pragma once

/**
 * @brief Runs automatic tests on @p package_path
 *
 * @param package_path main package folder path
 *
 * @return 0 on success, -1 on error
 */
int setLimits(const std::string& package_path);
