#pragma once

// TODO: delete this file when it becomes unneeded

#include <cstdint>
#include <optional>
#include <sim/old_sql_fields/blob.hh>
#include <sim/old_sql_fields/datetime.hh>
#include <sim/primary_key.hh>
#include <sim/users/user.hh>
#include <simlib/enum_val.hh>
#include <simlib/macros/enum_with_string_conversions.hh>
#include <simlib/string_view.hh>

namespace sim::jobs {

struct OldJob {
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
        (DELETE_FILE, 16, "delete_file")
        (CHANGE_PROBLEM_STATEMENT, 17, "change_problem_statement")
        (MERGE_USERS, 18, "merge_users")
    );

    ENUM_WITH_STRING_CONVERSIONS(Status, uint8_t,
        (PENDING, 1, "pending")
        (NOTICED_PENDING, 2, "noticed_pending")
        (IN_PROGRESS, 3, "in_progress")
        (DONE, 4, "done")
        (FAILED, 5, "failed")
        (CANCELED, 6, "canceled")
    );

    uint64_t id;
    old_sql_fields::Datetime created_at;
    std::optional<decltype(users::User::id)> creator;
    EnumVal<Type> type;
    uint8_t priority;
    EnumVal<Status> status;
    std::optional<uint64_t> aux_id;
    std::optional<uint64_t> aux_id_2;
    old_sql_fields::Blob<0> log;

    static constexpr auto primary_key = PrimaryKey{&OldJob::id};
};

constexpr uint64_t job_log_view_max_size = 128 << 10; // 128 KiB

// The greater, the more important
constexpr decltype(OldJob::priority) default_priority(OldJob::Type type) {
    switch (type) {
    case OldJob::Type::DELETE_FILE: return 40;
    case OldJob::Type::DELETE_PROBLEM:
    case OldJob::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
    case OldJob::Type::DELETE_USER:
    case OldJob::Type::DELETE_CONTEST:
    case OldJob::Type::DELETE_CONTEST_ROUND:
    case OldJob::Type::DELETE_CONTEST_PROBLEM: return 30;
    case OldJob::Type::MERGE_USERS:
    case OldJob::Type::MERGE_PROBLEMS:
    case OldJob::Type::EDIT_PROBLEM:
    case OldJob::Type::CHANGE_PROBLEM_STATEMENT: return 25;
    case OldJob::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION: return 20;
    case OldJob::Type::ADD_PROBLEM:
    case OldJob::Type::REUPLOAD_PROBLEM: return 10;
    case OldJob::Type::JUDGE_SUBMISSION: return 5;
    case OldJob::Type::REJUDGE_SUBMISSION: return 4;
    }
    __builtin_unreachable();
}

constexpr const char* to_string(OldJob::Type x) {
    using JT = OldJob::Type;
    switch (x) {
    case JT::JUDGE_SUBMISSION: return "JUDGE_SUBMISSION";
    case JT::ADD_PROBLEM: return "ADD_PROBLEM";
    case JT::REUPLOAD_PROBLEM: return "REUPLOAD_PROBLEM";
    case JT::EDIT_PROBLEM: return "EDIT_PROBLEM";
    case JT::DELETE_PROBLEM: return "DELETE_PROBLEM";
    case JT::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
        return "RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM";
    case JT::DELETE_USER: return "DELETE_USER";
    case JT::DELETE_CONTEST: return "DELETE_CONTEST";
    case JT::DELETE_CONTEST_ROUND: return "DELETE_CONTEST_ROUND";
    case JT::DELETE_CONTEST_PROBLEM: return "DELETE_CONTEST_PROBLEM";
    case JT::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
        return "RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION";
    case JT::MERGE_PROBLEMS: return "MERGE_PROBLEMS";
    case JT::REJUDGE_SUBMISSION: return "REJUDGE_SUBMISSION";
    case JT::DELETE_FILE: return "DELETE_FILE";
    case JT::CHANGE_PROBLEM_STATEMENT: return "CHANGE_PROBLEM_STATEMENT";
    case JT::MERGE_USERS: return "MERGE_USERS";
    }
    return "Unknown";
}

constexpr bool is_problem_management_job(OldJob::Type x) {
    using JT = OldJob::Type;
    switch (x) {
    case JT::ADD_PROBLEM:
    case JT::REUPLOAD_PROBLEM:
    case JT::EDIT_PROBLEM:
    case JT::DELETE_PROBLEM:
    case JT::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
    case JT::MERGE_PROBLEMS:
    case JT::CHANGE_PROBLEM_STATEMENT: return true;
    case JT::JUDGE_SUBMISSION:
    case JT::REJUDGE_SUBMISSION:
    case JT::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
    case JT::DELETE_USER:
    case JT::DELETE_CONTEST:
    case JT::DELETE_CONTEST_ROUND:
    case JT::DELETE_CONTEST_PROBLEM:
    case JT::DELETE_FILE:
    case JT::MERGE_USERS: return false;
    }
    return false;
}

constexpr bool is_submission_job(OldJob::Type x) {
    using JT = OldJob::Type;
    switch (x) {
    case JT::JUDGE_SUBMISSION:
    case JT::REJUDGE_SUBMISSION: return true;
    case JT::ADD_PROBLEM:
    case JT::REUPLOAD_PROBLEM:
    case JT::EDIT_PROBLEM:
    case JT::DELETE_PROBLEM:
    case JT::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
    case JT::DELETE_USER:
    case JT::DELETE_CONTEST:
    case JT::DELETE_CONTEST_ROUND:
    case JT::DELETE_CONTEST_PROBLEM:
    case JT::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
    case JT::MERGE_PROBLEMS:
    case JT::DELETE_FILE:
    case JT::CHANGE_PROBLEM_STATEMENT:
    case JT::MERGE_USERS: return false;
    }
    return false;
}

constexpr const char* to_string(OldJob::Status x) {
    switch (x) {
    case OldJob::Status::PENDING:
    case OldJob::Status::NOTICED_PENDING: return "Pending";
    case OldJob::Status::IN_PROGRESS: return "In progress";
    case OldJob::Status::DONE: return "Done";
    case OldJob::Status::FAILED: return "Failed";
    case OldJob::Status::CANCELED: return "Canceled";
    }
    return "Unknown";
}

} // namespace sim::jobs
