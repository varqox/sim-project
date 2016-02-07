#pragma once

#include "conver.h"

/**
 * @brief Validates whole package
 * @details Check if there is only one main folder and validate the config file,
 *   but only if it exists and the USE_CONFIG flag is set
 *
 * @param pathname path to package (not main folder)
 *
 * @return path to main folder (based on pathname)
 */
std::string validatePackage(std::string pathname);
