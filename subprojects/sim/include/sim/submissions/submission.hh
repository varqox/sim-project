#pragma once

#include "cstdint"
#include "optional"

#include <sim/contest_problems/contest_problem.hh>
#include <sim/contest_rounds/contest_round.hh>
#include <sim/contests/contest.hh>
#include <sim/internal_files/internal_file.hh>
#include <sim/problems/problem.hh>
#include <sim/sql/fields/blob.hh>
#include <sim/sql/fields/datetime.hh>
#include <sim/users/user.hh>
#include <simlib/macros/enum_with_string_conversions.hh>

namespace sim::submissions {

struct Submission {
    ENUM_WITH_STRING_CONVERSIONS(Type, uint8_t,
        (NORMAL, 0, "normal")
        (IGNORED, 2, "ignored")
        (PROBLEM_SOLUTION, 3, "problem_solution")
    );

    ENUM_WITH_STRING_CONVERSIONS(Language, uint8_t,
        (C11, 0, "c11")
        (CPP11, 1, "cpp11")
        (PASCAL, 2, "pascal")
        (CPP14, 3, "cpp14")
        (CPP17, 4, "cpp17")
        (PYTHON, 5, "python")
        (RUST, 6, "rust")
    );

    // Initial and final values may be combined, but special not
    ENUM_WITH_STRING_CONVERSIONS(Status, uint8_t,
        // Final
        (OK, 1, "ok")
        (WA, 2, "wa")
        (TLE, 3, "tle")
        (MLE, 4, "mle")
        (OLE, 5, "ole")
        (RTE, 6, "rte")
        // Special
        (PENDING, 8 + 0, "pending")
        // Fatal
        (COMPILATION_ERROR, 8 + 1, "compilation_error")
        (CHECKER_COMPILATION_ERROR, 8 + 2, "checker_compilation_error")
        (JUDGE_ERROR, 8 + 3, "judge_error")
    );

    uint64_t id;
    sql::fields::Datetime created_at;
    decltype(internal_files::InternalFile::id) file_id;
    std::optional<decltype(users::User::id)> owner;
    decltype(problems::Problem::id) problem_id;
    std::optional<decltype(contest_problems::ContestProblem::id)> contest_problem_id;
    std::optional<decltype(contest_rounds::ContestRound::id)> contest_round_id;
    std::optional<decltype(contests::Contest::id)> contest_id;
    Type type;
    Language language;
    bool final_candidate;
    bool problem_final;
    bool contest_final;
    bool contest_initial_final;
    Status initial_status;
    Status full_status;
    std::optional<int64_t> score;
    sql::fields::Datetime last_judgment;
    sql::fields::Blob initial_report;
    sql::fields::Blob final_report;

    static constexpr uint64_t SOLUTION_MAX_SIZE = 100 << 10; // 100 KiB
};

} // namespace sim::submissions
