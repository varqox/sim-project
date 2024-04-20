#pragma once

// TODO: delete this file when it becomes unneeded

#include <cstdint>
#include <sim/contests/old_contest.hh>
#include <sim/old_sql_fields/inf_datetime.hh>
#include <sim/old_sql_fields/varbinary.hh>
#include <sim/primary_key.hh>

namespace sim::contest_rounds {

struct OldContestRound {
    uint64_t id;
    decltype(contests::OldContest::id) contest_id;
    old_sql_fields::Varbinary<128> name;
    uint64_t item;
    old_sql_fields::InfDatetime begins;
    old_sql_fields::InfDatetime ends;
    old_sql_fields::InfDatetime full_results;
    old_sql_fields::InfDatetime ranking_exposure;

    static constexpr auto primary_key = PrimaryKey{&OldContestRound::id};
};

} // namespace sim::contest_rounds
