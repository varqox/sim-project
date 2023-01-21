#pragma once

#include <cstdint>
#include <sim/contest_rounds/contest_round.hh>
#include <sim/contests/contest.hh>
#include <sim/primary_key.hh>
#include <sim/problems/problem.hh>
#include <sim/sql_fields/varbinary.hh>
#include <simlib/enum_val.hh>
#include <simlib/enum_with_string_conversions.hh>
#include <simlib/string_view.hh>

namespace sim::contest_problems {

struct ContestProblem {
    ENUM_WITH_STRING_CONVERSIONS(MethodOfChoosingFinalSubmission, uint8_t,
        (LATEST_COMPILING, 0, "latest_compiling")
        (HIGHEST_SCORE, 1, "highest_score")
    );

    ENUM_WITH_STRING_CONVERSIONS(ScoreRevealing, uint8_t,
        (NONE, 0, "none")
        (ONLY_SCORE, 1, "only_score")
        (SCORE_AND_FULL_STATUS, 2, "score_and_full_status")
    );

    uint64_t id;
    decltype(contest_rounds::ContestRound::id) contest_round_id;
    decltype(contests::Contest::id) contest_id;
    decltype(problems::Problem::id) problem_id;
    sql_fields::Varbinary<128> name;
    uint64_t item;
    EnumVal<MethodOfChoosingFinalSubmission> method_of_choosing_final_submission;
    EnumVal<ScoreRevealing> score_revealing;

    constexpr static auto primary_key = PrimaryKey{&ContestProblem::id};
};

} // namespace sim::contest_problems
