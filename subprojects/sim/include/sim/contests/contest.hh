#pragma once

#include <cstdint>
#include <sim/sql/fields/varbinary.hh>

namespace sim::contests {

struct Contest {
    uint64_t id;
    sql::fields::Varbinary<128> name;
    bool is_public;
};

} // namespace sim::contests
