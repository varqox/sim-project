#pragma once

// TODO: delete this file when it becomes unneeded

#include <cstdint>
#include <optional>
#include <sim/contest_problems/old_contest_problem.hh>
#include <sim/contest_rounds/old_contest_round.hh>
#include <sim/contests/old_contest.hh>
#include <sim/internal_files/old_internal_file.hh>
#include <sim/old_sql_fields/blob.hh>
#include <sim/old_sql_fields/datetime.hh>
#include <sim/primary_key.hh>
#include <sim/problems/old_problem.hh>
#include <sim/users/old_user.hh>
#include <simlib/macros/enum_with_string_conversions.hh>
#include <simlib/meta/max.hh>
#include <simlib/meta/min.hh>

namespace sim::submissions {

struct OldSubmission {
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
        (CPP20, 7, "cpp20")
        (CPP23, 8, "cpp23")
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
    old_sql_fields::Datetime created_at;
    decltype(internal_files::OldInternalFile::id) file_id;
    std::optional<decltype(sim::users::User::id)> user_id;
    decltype(sim::problems::OldProblem::id) problem_id;
    std::optional<decltype(sim::contest_problems::OldContestProblem::id)> contest_problem_id;
    std::optional<decltype(sim::contest_rounds::OldContestRound::id)> contest_round_id;
    std::optional<decltype(sim::contests::OldContest::id)> contest_id;
    EnumVal<Type> type;
    EnumVal<Language> language;
    old_sql_fields::Bool final_candidate;
    old_sql_fields::Bool problem_final;
    old_sql_fields::Bool contest_problem_final;
    old_sql_fields::Bool contest_problem_initial_final;
    EnumVal<Status> initial_status;
    EnumVal<Status> full_status;
    std::optional<int64_t> score;
    std::optional<old_sql_fields::Datetime> last_judgment_began_at;
    old_sql_fields::Blob<0> initial_report;
    old_sql_fields::Blob<0> final_report;

    static constexpr auto primary_key = PrimaryKey{&OldSubmission::id};

    static constexpr uint64_t solution_max_size = 100 << 10; // 100 KiB
};

constexpr const char* to_string(OldSubmission::Type x) {
    switch (x) {
    case OldSubmission::Type::NORMAL: return "Normal";
    case OldSubmission::Type::IGNORED: return "Ignored";
    case OldSubmission::Type::PROBLEM_SOLUTION: return "Problem solution";
    }
    return "Unknown";
}

constexpr const char* to_string(OldSubmission::Language x) {
    switch (x) {
    case OldSubmission::Language::C11: return "C11";
    case OldSubmission::Language::CPP11: return "C++11";
    case OldSubmission::Language::CPP14: return "C++14";
    case OldSubmission::Language::CPP17: return "C++17";
    case OldSubmission::Language::CPP20: return "C++20";
    case OldSubmission::Language::CPP23: return "C++23";
    case OldSubmission::Language::PASCAL: return "Pascal";
    case OldSubmission::Language::PYTHON: return "Python";
    case OldSubmission::Language::RUST: return "Rust";
    }
    return "Unknown";
}

constexpr const char* to_extension(OldSubmission::Language x) {
    switch (x) {
    case OldSubmission::Language::C11: return ".c";
    case OldSubmission::Language::CPP11:
    case OldSubmission::Language::CPP14:
    case OldSubmission::Language::CPP17:
    case OldSubmission::Language::CPP20:
    case OldSubmission::Language::CPP23: return ".cpp";
    case OldSubmission::Language::PASCAL: return ".pas";
    case OldSubmission::Language::PYTHON: return ".py";
    case OldSubmission::Language::RUST: return ".rs";
    }
    return "Unknown";
}

constexpr const char* to_mime(OldSubmission::Language x) {
    switch (x) {
    case OldSubmission::Language::C11: return "text/x-csrc";
    case OldSubmission::Language::CPP11:
    case OldSubmission::Language::CPP14:
    case OldSubmission::Language::CPP17:
    case OldSubmission::Language::CPP20:
    case OldSubmission::Language::CPP23: return "text/x-c++src";
    case OldSubmission::Language::PASCAL: return "text/x-pascal";
    case OldSubmission::Language::PYTHON: return "text/x-python";
    case OldSubmission::Language::RUST: return "text/x-rust";
    }
    return "Unknown";
}

// Non-fatal statuses
static_assert(
    meta::max(
        OldSubmission::Status::OK,
        OldSubmission::Status::WA,
        OldSubmission::Status::TLE,
        OldSubmission::Status::MLE,
        OldSubmission::Status::OLE,
        OldSubmission::Status::RTE
    ) < OldSubmission::Status::PENDING,
    "Needed as a boundary between non-fatal and fatal statuses - it is strongly"
    " used during selection of the final submission"
);

// Fatal statuses
static_assert(
    meta::min(
        OldSubmission::Status::COMPILATION_ERROR,
        OldSubmission::Status::CHECKER_COMPILATION_ERROR,
        OldSubmission::Status::JUDGE_ERROR
    ) > OldSubmission::Status::PENDING,
    "Needed as a boundary between non-fatal and fatal statuses - it is strongly"
    " used during selection of the final submission"
);

constexpr bool is_special(OldSubmission::Status status) {
    return (status >= OldSubmission::Status::PENDING);
}

constexpr bool is_fatal(OldSubmission::Status status) {
    return (status > OldSubmission::Status::PENDING);
}

constexpr const char* css_color_class(OldSubmission::Status status) noexcept {
    switch (status) {
    case OldSubmission::Status::OK: return "green";
    case OldSubmission::Status::WA: return "red";
    case OldSubmission::Status::TLE:
    case OldSubmission::Status::MLE:
    case OldSubmission::Status::OLE: return "yellow";
    case OldSubmission::Status::RTE: return "intense-red";
    case OldSubmission::Status::PENDING: return "";
    case OldSubmission::Status::COMPILATION_ERROR: return "purple";
    case OldSubmission::Status::CHECKER_COMPILATION_ERROR:
    case OldSubmission::Status::JUDGE_ERROR: return "blue";
    }

    return ""; // Shouldn't happen
}

} // namespace sim::submissions
