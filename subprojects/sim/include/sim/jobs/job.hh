#pragma once

#include <cstdint>
#include <optional>
#include <sim/sql/fields/blob.hh>
#include <sim/sql/fields/datetime.hh>
#include <sim/users/user.hh>
#include <simlib/macros/enum_with_string_conversions.hh>

namespace sim::jobs {

struct Job {
    ENUM_WITH_STRING_CONVERSIONS(Type, uint8_t,
        (JUDGE_SUBMISSION, 1, "judge_submission")
        (ADD_PROBLEM, 2, "add_problem")
        (REUPLOAD_PROBLEM, 3, "reupload_problem")
        (EDIT_PROBLEM, 6, "edit_problem")
        (DELETE_PROBLEM, 7, "delete_problem")
        (RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM, 8,
            "reselect_final_submissions_in_contest_problem")
        (DELETE_USER, 9, "delete_user")
        (DELETE_CONTEST, 10, "delete_contest")
        (DELETE_CONTEST_ROUND, 11, "delete_contest_round")
        (DELETE_CONTEST_PROBLEM, 12, "delete_contest_problem")
        (RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION, 13,
            "reset_problem_time_limits_using_model_solution")
        (MERGE_PROBLEMS, 14, "merge_problems")
        (REJUDGE_SUBMISSION, 15, "rejudge_submission")
        (DELETE_INTERNAL_FILE, 16, "delete_internal_file")
        (CHANGE_PROBLEM_STATEMENT, 17, "change_problem_statement")
        (MERGE_USERS, 18, "merge_users")
    );

    ENUM_WITH_STRING_CONVERSIONS(Status, uint8_t,
        (PENDING, 1, "pending")
        (IN_PROGRESS, 3, "in_progress")
        (DONE, 4, "done")
        (FAILED, 5, "failed")
        (CANCELLED, 6, "cancelled")
    );

    uint64_t id;
    sql::fields::Datetime created_at;
    std::optional<decltype(users::User::id)> creator;
    Type type;
    uint8_t priority;
    Status status;
    std::optional<uint64_t> aux_id;
    std::optional<uint64_t> aux_id_2;
    sql::fields::Blob log;
    static constexpr size_t COLUMNS_NUM = 9;
};

// The greater, the more important
constexpr decltype(Job::priority) default_priority(Job::Type type) {
    // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
    switch (type) {
    case Job::Type::DELETE_INTERNAL_FILE: return 40;
    case Job::Type::DELETE_PROBLEM:
    case Job::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
    case Job::Type::DELETE_USER:
    case Job::Type::DELETE_CONTEST:
    case Job::Type::DELETE_CONTEST_ROUND:
    case Job::Type::DELETE_CONTEST_PROBLEM: return 30;
    case Job::Type::MERGE_USERS:
    case Job::Type::MERGE_PROBLEMS:
    case Job::Type::EDIT_PROBLEM:
    case Job::Type::CHANGE_PROBLEM_STATEMENT: return 25;
    case Job::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION: return 20;
    case Job::Type::ADD_PROBLEM:
    case Job::Type::REUPLOAD_PROBLEM: return 10;
    case Job::Type::JUDGE_SUBMISSION: return 5;
    case Job::Type::REJUDGE_SUBMISSION: return 4;
    }
    THROW("Invalid job type");
}

} // namespace sim::jobs
