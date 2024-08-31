#pragma once

#include <cstdint>
#include <sim/contests/contest.hh>
#include <sim/sql/fields/datetime.hh>
#include <sim/sql/fields/inf_datetime.hh>
#include <sim/sql/fields/varbinary.hh>

namespace sim::contest_rounds {

struct ContestRound {
    uint64_t id;
    sql::fields::Datetime created_at;
    decltype(contests::Contest::id) contest_id;
    sql::fields::Varbinary<128> name;
    uint64_t item;
    sql::fields::InfDatetime begins;
    sql::fields::InfDatetime ends;
    sql::fields::InfDatetime full_results;
    sql::fields::InfDatetime ranking_exposure;
    static constexpr size_t COLUMNS_NUM = 9;
};

} // namespace sim::contest_rounds
