#pragma once

#include "simlib/mysql/mysql.hh"

#include <array>
#include <climits>

namespace sim_merger {

inline mysql::Connection conn;

inline InplaceBuff<PATH_MAX> main_sim_build;
inline InplaceBuff<PATH_MAX> other_sim_build;

constexpr StringView main_sim_table_prefix = "main_sim_";

} // namespace sim_merger
