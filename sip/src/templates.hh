#pragma once

#include <optional>
#include <simlib/string_view.hh>

namespace templates {

std::string checker_cc();

std::string interactive_checker_cc();

std::string sim_statement_cls();

// Specify memory_limit in bytes
std::string statement_tex(StringView problem_name,
                          std::optional<uint64_t> memory_limit);

std::string gen_cc();

} // namespace templates
