#pragma once

// TODO: delete this file when it becomes unneeded

#include <cstdint>
#include <sim/contest_rounds/old_contest_round.hh>
#include <sim/contests/old_contest.hh>
#include <sim/old_sql_fields/varbinary.hh>
#include <sim/primary_key.hh>
#include <sim/problems/old_problem.hh>
#include <simlib/enum_val.hh>
#include <simlib/macros/enum_with_string_conversions.hh>
#include <simlib/string_view.hh>

namespace sim::contest_problems {

struct OldContestProblem {
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
    decltype(contest_rounds::OldContestRound::id) contest_round_id;
    decltype(contests::OldContest::id) contest_id;
    decltype(problems::OldProblem::id) problem_id;
    old_sql_fields::Varbinary<128> name;
    uint64_t item;
    EnumVal<MethodOfChoosingFinalSubmission> method_of_choosing_final_submission;
    EnumVal<ScoreRevealing> score_revealing;

    static constexpr auto primary_key = PrimaryKey{&OldContestProblem::id};
};

} // namespace sim::contest_problems
