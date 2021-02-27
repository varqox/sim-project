#pragma once

#include <string>

namespace sim {

/// Generates token of length @p length which consist of [a-zA-Z0-9]
std::string generate_random_token(size_t length);

} // namespace sim
