#pragma once

#include "conver.h"

/**
 * @brief Validates whole package
 * @details Check if there is only one main folder and validates conf.cfg if
 * exists and flag USE_CONF is set
 *
 * @param pathname path to package (not main folder)
 * @return path to main folder (based on pathname)
 */
std::string validatePackage(std::string pathname);
