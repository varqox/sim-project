#pragma once

#include <string_view>
#include <vector>

namespace sim::db {

constexpr size_t TABLES_NUM = 18;

// Tables in topological order (every table depends only on the previous tables)
const std::vector<std::string_view>& get_tables();

} // namespace sim::db
