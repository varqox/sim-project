#pragma once

#include <array>
#include <simlib/string_view.hh>

// Tables in topological order (every table depends only on the previous tables)
constexpr std::array<CStringView, 13> tables = {{
    "internal_files",
    "users",
    "sessions",
    "problems",
    "problem_tags",
    "contests",
    "contest_rounds",
    "contest_problems",
    "contest_users",
    "contest_files",
    "contest_entry_tokens",
    "submissions",
    "jobs",
}};
