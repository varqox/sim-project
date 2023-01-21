#pragma once

#include <cstdint>
#include <sim/primary_key.hh>
#include <sim/sql_fields/bool.hh>
#include <sim/sql_fields/varbinary.hh>

namespace sim::contests {

struct Contest {
    uint64_t id;
    sql_fields::Varbinary<128> name;
    sql_fields::Bool is_public;

    constexpr static auto primary_key = PrimaryKey{&Contest::id};
};

} // namespace sim::contests
