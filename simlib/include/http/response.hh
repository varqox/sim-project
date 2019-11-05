#pragma once

#include "../string_view.hh"

namespace http {

/// Returns quoted string that can be placed safely in a http header
std::string quote(StringView str);

} // namespace http
