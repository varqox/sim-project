#pragma once

#include "../string.h"

namespace http {

/// Returns quoted string that can be placed safely in a http header
std::string quote(StringView str);

} // namespace http
