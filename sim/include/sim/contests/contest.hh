#pragma once

#include "sim/sql_fields/varchar.hh"

#include <cstdint>

namespace sim::contests {

struct Contest {
    uint64_t id;
    sql_fields::Varchar<128> name;
    bool is_public;
};

} // namespace sim::contests
