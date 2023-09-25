#pragma once

#include <cstdint>
#include <optional>
#include <sim/contest_problems/contest_problem.hh>
#include <sim/contest_rounds/contest_round.hh>
#include <sim/contests/contest.hh>
#include <sim/internal_files/internal_file.hh>
#include <sim/primary_key.hh>
#include <sim/problems/problem.hh>
#include <sim/sql_fields/blob.hh>
#include <sim/sql_fields/datetime.hh>
#include <sim/users/user.hh>
#include <simlib/macros/enum_with_string_conversions.hh>
#include <simlib/meta/max.hh>
#include <simlib/meta/min.hh>

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
    decltype(internal_files::InternalFile::id) file_id;
    std::optional<decltype(sim::users::User::id)> owner;
    decltype(sim::problems::Problem::id) problem_id;
    std::optional<decltype(sim::contest_problems::ContestProblem::id)> contest_problem_id;
    std::optional<decltype(sim::contest_rounds::ContestRound::id)> contest_round_id;
    std::optional<decltype(sim::contests::Contest::id)> contest_id;
    EnumVal<Type> type;
    EnumVal<Language> language;
    sql_fields::Bool final_candidate;
    sql_fields::Bool problem_final;
    sql_fields::Bool contest_final;
    sql_fields::Bool contest_initial_final;
    EnumVal<Status> initial_status;
    EnumVal<Status> full_status;
    sql_fields::Datetime submit_time;
    std::optional<int64_t> score;
    sql_fields::Datetime last_judgment;
    sql_fields::Blob<0> initial_report;
    sql_fields::Blob<0> final_report;

    static constexpr auto primary_key = PrimaryKey{&Submission::id};

    static constexpr uint64_t solution_max_size = 100 << 10; // 100 KiB
};

constexpr const char* to_string(Submission::Type x) {
    switch (x) {
    case Submission::Type::NORMAL: return "Normal";
    case Submission::Type::IGNORED: return "Ignored";
    case Submission::Type::PROBLEM_SOLUTION: return "Problem solution";
    }
    return "Unknown";
}

constexpr const char* to_string(Submission::Language x) {
    switch (x) {
    case Submission::Language::C11: return "C11";
    case Submission::Language::CPP11: return "C++11";
    case Submission::Language::CPP14: return "C++14";
    case Submission::Language::CPP17: return "C++17";
    case Submission::Language::PASCAL: return "Pascal";
    }
    return "Unknown";
}

constexpr const char* to_extension(Submission::Language x) {
    switch (x) {
    case Submission::Language::C11: return ".c";
    case Submission::Language::CPP11:
    case Submission::Language::CPP14:
    case Submission::Language::CPP17: return ".cpp";
    case Submission::Language::PASCAL: return ".pas";
    }
    return "Unknown";
}

constexpr const char* to_mime(Submission::Language x) {
    switch (x) {
    case Submission::Language::C11: return "text/x-csrc";
    case Submission::Language::CPP11:
    case Submission::Language::CPP14:
    case Submission::Language::CPP17: return "text/x-c++src";
    case Submission::Language::PASCAL: return "text/x-pascal";
    }
    return "Unknown";
}

// Non-fatal statuses
static_assert(
    meta::max(
        Submission::Status::OK,
        Submission::Status::WA,
        Submission::Status::TLE,
        Submission::Status::MLE,
        Submission::Status::OLE,
        Submission::Status::RTE
    ) < Submission::Status::PENDING,
    "Needed as a boundary between non-fatal and fatal statuses - it is strongly"
    " used during selection of the final submission"
);

// Fatal statuses
static_assert(
    meta::min(
        Submission::Status::COMPILATION_ERROR,
        Submission::Status::CHECKER_COMPILATION_ERROR,
        Submission::Status::JUDGE_ERROR
    ) > Submission::Status::PENDING,
    "Needed as a boundary between non-fatal and fatal statuses - it is strongly"
    " used during selection of the final submission"
);

constexpr bool is_special(Submission::Status status) {
    return (status >= Submission::Status::PENDING);
}

constexpr bool is_fatal(Submission::Status status) {
    return (status > Submission::Status::PENDING);
}

constexpr const char* css_color_class(Submission::Status status) noexcept {
    switch (status) {
    case Submission::Status::OK: return "green";
    case Submission::Status::WA: return "red";
    case Submission::Status::TLE:
    case Submission::Status::MLE:
    case Submission::Status::OLE: return "yellow";
    case Submission::Status::RTE: return "intense-red";
    case Submission::Status::PENDING: return "";
    case Submission::Status::COMPILATION_ERROR: return "purple";
    case Submission::Status::CHECKER_COMPILATION_ERROR:
    case Submission::Status::JUDGE_ERROR: return "blue";
    }

    return ""; // Shouldn't happen
}

} // namespace sim::submissions
