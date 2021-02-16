#pragma once

#include "simlib/mysql.hh"

#include <array>
#include <climits>

inline MySQL::Connection conn;

inline InplaceBuff<PATH_MAX> main_sim_build;
inline InplaceBuff<PATH_MAX> other_sim_build;

constexpr StringView main_sim_table_prefix = "main_sim_";
constexpr std::array<CStringView, 13> tables = {{
    "contest_entry_tokens",
    "contest_files",
    "contest_problems",
    "contest_rounds",
    "contest_users",
    "contests",
    "internal_files",
    "jobs",
    "problem_tags",
    "problems",
    "session",
    "submissions",
    "users",
}};
