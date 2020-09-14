#pragma once

#include "simlib/string_view.hh"

namespace http {

/// Returns quoted string that can be placed safely in a http header
std::string quote(const StringView& str);

class Response {};

} // namespace http
