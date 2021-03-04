#pragma once

#include "sim/internal_files/internal_file.hh"
#include "sim/sql_fields/blob.hh"
#include "sim/sql_fields/datetime.hh"
#include "sim/users/user.hh"
#include "simlib/string_view.hh"

#include <cstdint>
#include <optional>

namespace sim::jobs {

struct Job {
    enum class Type : uint8_t {
        JUDGE_SUBMISSION = 1,
        ADD_PROBLEM = 2,
        REUPLOAD_PROBLEM = 3,
        ADD_PROBLEM__JUDGE_MODEL_SOLUTION = 4,
        REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION = 5,
        EDIT_PROBLEM = 6,
        DELETE_PROBLEM = 7,
        RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM = 8,
        DELETE_USER = 9,
        DELETE_CONTEST = 10,
        DELETE_CONTEST_ROUND = 11,
        DELETE_CONTEST_PROBLEM = 12,
        RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION = 13,
        MERGE_PROBLEMS = 14,
        REJUDGE_SUBMISSION = 15,
        DELETE_FILE = 16,
        CHANGE_PROBLEM_STATEMENT = 17,
        MERGE_USERS = 18,
    };

    enum class Status : uint8_t {
        PENDING = 1,
        NOTICED_PENDING = 2,
        IN_PROGRESS = 3,
        DONE = 4,
        FAILED = 5,
        CANCELED = 6
    };

    uint64_t id;
    std::optional<decltype(internal_files::InternalFile::id)> file_id;
    std::optional<decltype(internal_files::InternalFile::id)> tmp_file_id;
    std::optional<decltype(users::User::id)> creator;
    EnumVal<Type> type;
    uint8_t priority;
    EnumVal<Status> status;
    sql_fields::Datetime added;
    std::optional<uint64_t> aux_id;
    sql_fields::Blob<128> info;
    sql_fields::Blob<1> data;
};

constexpr uint64_t job_log_view_max_size = 128 << 10; // 128 KiB

// The greater, the more important
constexpr decltype(Job::priority) default_priority(Job::Type type) {
    switch (type) {
    case Job::Type::DELETE_FILE: return 40;
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
    case Job::Type::ADD_PROBLEM__JUDGE_MODEL_SOLUTION:
    case Job::Type::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION: return 15;
    case Job::Type::ADD_PROBLEM:
    case Job::Type::REUPLOAD_PROBLEM: return 10;
    case Job::Type::JUDGE_SUBMISSION: return 5;
    case Job::Type::REJUDGE_SUBMISSION: return 4;
    }
    __builtin_unreachable();
}

constexpr const char* to_string(Job::Type x) {
    using JT = Job::Type;
    switch (x) {
    case JT::JUDGE_SUBMISSION: return "JUDGE_SUBMISSION";
    case JT::ADD_PROBLEM: return "ADD_PROBLEM";
    case JT::REUPLOAD_PROBLEM: return "REUPLOAD_PROBLEM";
    case JT::ADD_PROBLEM__JUDGE_MODEL_SOLUTION: return "ADD_PROBLEM__JUDGE_MODEL_SOLUTION";
    case JT::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION:
        return "REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION";
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

constexpr bool is_problem_management_job(Job::Type x) {
    using JT = Job::Type;
    switch (x) {
    case JT::ADD_PROBLEM:
    case JT::ADD_PROBLEM__JUDGE_MODEL_SOLUTION:
    case JT::REUPLOAD_PROBLEM:
    case JT::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION:
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

constexpr bool is_submission_job(Job::Type x) {
    using JT = Job::Type;
    switch (x) {
    case JT::JUDGE_SUBMISSION:
    case JT::REJUDGE_SUBMISSION: return true;
    case JT::ADD_PROBLEM:
    case JT::ADD_PROBLEM__JUDGE_MODEL_SOLUTION:
    case JT::REUPLOAD_PROBLEM:
    case JT::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION:
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

constexpr const char* to_string(Job::Status x) {
    switch (x) {
    case Job::Status::PENDING:
    case Job::Status::NOTICED_PENDING: return "Pending";
    case Job::Status::IN_PROGRESS: return "In progress";
    case Job::Status::DONE: return "Done";
    case Job::Status::FAILED: return "Failed";
    case Job::Status::CANCELED: return "Canceled";
    }
    return "Unknown";
}

} // namespace sim::jobs
