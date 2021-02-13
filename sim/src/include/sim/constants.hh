#pragma once

#include <chrono>
#include <cstdint>
#include <simlib/concat.hh>
#include <simlib/meta.hh>

using uint = unsigned;

// Session
constexpr uint SESSION_ID_LEN = 30;
constexpr uint SESSION_CSRF_TOKEN_LEN = 20;
constexpr uint SESSION_IP_LEN = 15;
constexpr uint TMP_SESSION_MAX_LIFETIME = 60 * 60; // 1 hour [s]
constexpr uint SESSION_MAX_LIFETIME = 30 * 24 * 60 * 60; // 30 days [s]

// Problems
constexpr uint NEW_STATEMENT_MAX_SIZE = 10 << 20; // 10 MiB

// Problems' tags
constexpr uint PROBLEM_TAG_MAX_LEN = 128;

// Contest entry tokens
constexpr uint CONTEST_ENTRY_TOKEN_LEN = 48;
constexpr uint CONTEST_ENTRY_SHORT_TOKEN_LEN = 8;
constexpr uint CONTEST_ENTRY_SHORT_TOKEN_MAX_LIFETIME = 60 * 60; // 1 hour [s]

// Files
constexpr uint FILE_ID_LEN = 30;
constexpr uint FILE_NAME_MAX_LEN = 128;
constexpr uint FILE_DESCRIPTION_MAX_LEN = 512;
constexpr uint FILE_MAX_SIZE = 128 << 20; // 128 MiB

// Submissions
constexpr uint SOLUTION_MAX_SIZE = 100 << 10; // 100 Kib

enum class SubmissionType : uint8_t {
    NORMAL = 0,
    IGNORED = 2,
    PROBLEM_SOLUTION = 3,
};

constexpr const char* to_string(SubmissionType x) {
    switch (x) {
    case SubmissionType::NORMAL: return "Normal";
    case SubmissionType::IGNORED: return "Ignored";
    case SubmissionType::PROBLEM_SOLUTION: return "Problem solution";
    }
    return "Unknown";
}

enum class SubmissionLanguage : uint8_t {
    C11 = 0,
    CPP11 = 1,
    PASCAL = 2,
    CPP14 = 3,
    CPP17 = 4,
};

constexpr const char* to_string(SubmissionLanguage x) {
    switch (x) {
    case SubmissionLanguage::C11: return "C11";
    case SubmissionLanguage::CPP11: return "C++11";
    case SubmissionLanguage::CPP14: return "C++14";
    case SubmissionLanguage::CPP17: return "C++17";
    case SubmissionLanguage::PASCAL: return "Pascal";
    }
    return "Unknown";
}

constexpr const char* to_extension(SubmissionLanguage x) {
    switch (x) {
    case SubmissionLanguage::C11: return ".c";
    case SubmissionLanguage::CPP11:
    case SubmissionLanguage::CPP14:
    case SubmissionLanguage::CPP17: return ".cpp";
    case SubmissionLanguage::PASCAL: return ".pas";
    }
    return "Unknown";
}

constexpr const char* to_mime(SubmissionLanguage x) {
    switch (x) {
    case SubmissionLanguage::C11: return "text/x-csrc";
    case SubmissionLanguage::CPP11:
    case SubmissionLanguage::CPP14:
    case SubmissionLanguage::CPP17: return "text/x-c++src";
    case SubmissionLanguage::PASCAL: return "text/x-pascal";
    }
    return "Unknown";
}

// Initial and final values may be combined, but special not
enum class SubmissionStatus : uint8_t {
    // Final
    OK = 1,
    WA = 2,
    TLE = 3,
    MLE = 4,
    RTE = 5,
    // Special
    PENDING = 8 + 0,
    // Fatal
    COMPILATION_ERROR = 8 + 1,
    CHECKER_COMPILATION_ERROR = 8 + 2,
    JUDGE_ERROR = 8 + 3
};

DECLARE_ENUM_UNARY_OPERATOR(SubmissionStatus, ~)
DECLARE_ENUM_OPERATOR(SubmissionStatus, |)
DECLARE_ENUM_OPERATOR(SubmissionStatus, &)

// Non-fatal statuses
static_assert(
    meta::max(
        SubmissionStatus::OK, SubmissionStatus::WA, SubmissionStatus::TLE,
        SubmissionStatus::MLE, SubmissionStatus::RTE) < SubmissionStatus::PENDING,
    "Needed as a boundary between non-fatal and fatal statuses - it is strongly"
    " used during selection of the final submission");

// Fatal statuses
static_assert(
    meta::min(
        SubmissionStatus::COMPILATION_ERROR, SubmissionStatus::CHECKER_COMPILATION_ERROR,
        SubmissionStatus::JUDGE_ERROR) > SubmissionStatus::PENDING,
    "Needed as a boundary between non-fatal and fatal statuses - it is strongly"
    " used during selection of the final submission");

constexpr bool is_special(SubmissionStatus status) {
    return (status >= SubmissionStatus::PENDING);
}

constexpr bool is_fatal(SubmissionStatus status) {
    return (status > SubmissionStatus::PENDING);
}

constexpr const char* css_color_class(SubmissionStatus status) noexcept {
    switch (status) {
    case SubmissionStatus::OK: return "green";
    case SubmissionStatus::WA: return "red";
    case SubmissionStatus::TLE:
    case SubmissionStatus::MLE: return "yellow";
    case SubmissionStatus::RTE: return "intense-red";
    case SubmissionStatus::PENDING: return "";
    case SubmissionStatus::COMPILATION_ERROR: return "purple";
    case SubmissionStatus::CHECKER_COMPILATION_ERROR:
    case SubmissionStatus::JUDGE_ERROR: return "blue";
    }

    return ""; // Shouldn't happen
}

enum class JobType : uint8_t {
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

constexpr const char* to_string(JobType x) {
    using JT = JobType;
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

constexpr bool is_problem_management_job(JobType x) {
    using JT = JobType;
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

constexpr bool is_submission_job(JobType x) {
    using JT = JobType;
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

// The greater, the more important
constexpr uint priority(JobType x) {
    using JT = JobType;
    switch (x) {
    case JT::DELETE_FILE: return 40;
    case JT::DELETE_PROBLEM:
    case JT::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
    case JT::DELETE_USER:
    case JT::DELETE_CONTEST:
    case JT::DELETE_CONTEST_ROUND:
    case JT::DELETE_CONTEST_PROBLEM: return 30;
    case JT::MERGE_USERS:
    case JT::MERGE_PROBLEMS:
    case JT::EDIT_PROBLEM:
    case JT::CHANGE_PROBLEM_STATEMENT: return 25;
    case JT::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION: return 20;
    case JT::ADD_PROBLEM__JUDGE_MODEL_SOLUTION:
    case JT::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION: return 15;
    case JT::ADD_PROBLEM:
    case JT::REUPLOAD_PROBLEM: return 10;
    case JT::JUDGE_SUBMISSION: return 5;
    case JT::REJUDGE_SUBMISSION: return 4;
    }
    return 0;
}

enum class JobStatus : uint8_t {
    PENDING = 1,
    NOTICED_PENDING = 2,
    IN_PROGRESS = 3,
    DONE = 4,
    FAILED = 5,
    CANCELED = 6
};

constexpr const char* to_string(JobStatus x) {
    switch (x) {
    case JobStatus::PENDING:
    case JobStatus::NOTICED_PENDING: return "Pending";
    case JobStatus::IN_PROGRESS: return "In progress";
    case JobStatus::DONE: return "Done";
    case JobStatus::FAILED: return "Failed";
    case JobStatus::CANCELED: return "Canceled";
    }
    return "Unknown";
}

// Internal files
constexpr const char INTERNAL_FILES_DIR[] = "internal_files/";

template <class T>
auto internal_file_path(T file_id) {
    return concat<64>(INTERNAL_FILES_DIR, file_id);
}

// Jobs
constexpr uint JOB_LOG_VIEW_MAX_LENGTH = 128 << 10; // 128 KiB

// Logs
constexpr const char SERVER_LOG[] = "logs/server.log";
constexpr const char SERVER_ERROR_LOG[] = "logs/server-error.log";
constexpr const char JOB_SERVER_LOG[] = "logs/job-server.log";
constexpr const char JOB_SERVER_ERROR_LOG[] = "logs/job-server-error.log";
// Logs API
constexpr uint LOGS_FIRST_CHUNK_MAX_LEN = 16 << 10; // 16 KiB
constexpr uint LOGS_OTHER_CHUNK_MAX_LEN = 128 << 10; // 128 KiB

// API
constexpr uint API_FIRST_QUERY_ROWS_LIMIT = 50;
constexpr uint API_OTHER_QUERY_ROWS_LIMIT = 200;

// Job server notifying file
constexpr const char JOB_SERVER_NOTIFYING_FILE[] = ".job-server.notify";

constexpr uint COMPILATION_ERRORS_MAX_LENGTH = 16 << 10; // 32 KiB
constexpr std::chrono::nanoseconds SOLUTION_COMPILATION_TIME_LIMIT = std::chrono::seconds(30);
constexpr std::chrono::nanoseconds CHECKER_COMPILATION_TIME_LIMIT = std::chrono::seconds(30);
// Conver::ResetTimeLimitsOptions and Conver::Options
constexpr std::chrono::nanoseconds MIN_TIME_LIMIT = std::chrono::milliseconds(300);
constexpr std::chrono::nanoseconds MAX_TIME_LIMIT = std::chrono::seconds(22);
constexpr double SOLUTION_RUNTIME_COEFFICIENT = 3;
// JudgeWorker
constexpr std::chrono::nanoseconds CHECKER_TIME_LIMIT = std::chrono::seconds(22);
constexpr uint64_t CHECKER_MEMORY_LIMIT = 512 << 20; // 256 MiB
constexpr double SCORE_CUT_LAMBDA = 2. / 3.; // See JudgeWorker::score_cut_lambda
constexpr const char PROOT_PATH[] = "./proot";
