#pragma once

#include <cstdint>
#include <sim/contest_rounds/contest_round.hh>
#include <sim/contests/contest.hh>
#include <sim/problems/problem.hh>
#include <sim/sql/fields/datetime.hh>
#include <sim/sql/fields/varbinary.hh>
#include <simlib/macros/enum_with_string_conversions.hh>

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
    sql::fields::Datetime created_at;
    decltype(contest_rounds::ContestRound::id) contest_round_id;
    decltype(contests::Contest::id) contest_id;
    decltype(problems::Problem::id) problem_id;
    sql::fields::Varbinary<128> name;
    uint64_t item;
    MethodOfChoosingFinalSubmission method_of_choosing_final_submission;
    ScoreRevealing score_revealing;
    static constexpr size_t COLUMNS_NUM = 9;
};

} // namespace sim::contest_problems
