#pragma once

#include <cstdint>
#include <sim/sql/fields/datetime.hh>
#include <sim/sql/fields/varbinary.hh>

namespace sim::contests {

struct Contest {
    uint64_t id;
    sql::fields::Datetime created_at;
    sql::fields::Varbinary<128> name;
    bool is_public;
    static constexpr size_t COLUMNS_NUM = 4;
};

} // namespace sim::contests
