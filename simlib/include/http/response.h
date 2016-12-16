#pragma once

#include "../string.h"

namespace http {

/// Returns quoted string that can be placed safely in a http header
std::string quote(const StringView& str);

} // namespace http
