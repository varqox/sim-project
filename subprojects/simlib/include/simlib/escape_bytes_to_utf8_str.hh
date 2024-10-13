#pragma once

#include <string>
#include <string_view>

std::string escape_bytes_to_utf8_str(
    const std::string_view& prefix, const std::string_view& bytes, const std::string_view& suffix
);
