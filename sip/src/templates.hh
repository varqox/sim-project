#pragma once

#include <optional>
#include <simlib/string_view.hh>

namespace templates {

// Specify memory_limit in bytes
std::string statement_tex(StringView problem_name,
                          std::optional<uint64_t> memory_limit);

std::string checker_cc();

std::string interactive_checker_cc();

} // namespace templates
