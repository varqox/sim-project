#pragma once

#include "conver.h"

/**
 * @brief Validates conf.cfg file
 * @details Checks all fields
 *
 * @param package_path path to package main folder
 * @return 0 on success, -1 on error
 */
int validateConf(std::string package_path);

/**
 * @brief Validates whole package
 * @details Check if there is only one main folder and validates conf.cfg if
 * exists and flag USE_CONF is set
 *
 * @param pathname path to package (not main folder)
 * @return path to main folder (based on pathname)
 */
std::string validatePackage(std::string pathname);
