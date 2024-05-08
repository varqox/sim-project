#pragma once

// TODO: delete this file when it becomes unneeded

#include <cstdint>
#include <sim/old_sql_fields/bool.hh>
#include <sim/old_sql_fields/varbinary.hh>
#include <sim/primary_key.hh>

namespace sim::contests {

struct OldContest {
    uint64_t id;
    old_sql_fields::Varbinary<128> name;
    old_sql_fields::Bool is_public;

    static constexpr auto primary_key = PrimaryKey{&OldContest::id};
};

} // namespace sim::contests
