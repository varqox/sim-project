#pragma once

#include <array>
#include <climits>
#include <memory>
#include <sim/mysql/mysql.hh>

namespace sim_merger {

inline std::unique_ptr<sim::mysql::Connection> mysql;

inline InplaceBuff<PATH_MAX> main_sim_build;
inline InplaceBuff<PATH_MAX> other_sim_build;

constexpr StringView main_sim_table_prefix = "main_sim_";

} // namespace sim_merger
