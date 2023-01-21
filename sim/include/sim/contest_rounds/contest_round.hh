#pragma once

#include <cstdint>
#include <sim/contests/contest.hh>
#include <sim/primary_key.hh>
#include <sim/sql_fields/inf_datetime.hh>
#include <sim/sql_fields/varbinary.hh>

namespace sim::contest_rounds {

struct ContestRound {
    uint64_t id;
    decltype(contests::Contest::id) contest_id;
    sql_fields::Varbinary<128> name;
    uint64_t item;
    sql_fields::InfDatetime begins;
    sql_fields::InfDatetime ends;
    sql_fields::InfDatetime full_results;
    sql_fields::InfDatetime ranking_exposure;

    constexpr static auto primary_key = PrimaryKey{&ContestRound::id};
};

} // namespace sim::contest_rounds
