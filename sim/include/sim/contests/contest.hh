#pragma once

#include "sim/sql_fields/varbinary.hh"

#include <cstdint>

namespace sim::contests {

struct Contest {
    uint64_t id;
    sql_fields::Varbinary<128> name;
    bool is_public;
};

} // namespace sim::contests
