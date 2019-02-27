#pragma once

#include <chrono>
#include <cstdint>
#include <simlib/string.h>

// User
constexpr uint USERNAME_MAX_LEN = 30;
constexpr uint USER_FIRST_NAME_MAX_LEN = 60;
constexpr uint USER_LAST_NAME_MAX_LEN = 60;
constexpr uint USER_EMAIL_MAX_LEN = 60;
constexpr uint SALT_LEN = 64;
constexpr uint PASSWORD_HASH_LEN = 128;
constexpr uintmax_t MAX_UID = 4294967295;

#define SIM_ROOT_UID "1"

// user_type - strongly used -> do not change!
enum class UserType : uint8_t {
	ADMIN = 0,
	TEACHER = 1,
	NORMAL = 2
};

#define UTYPE_ADMIN_STR "0"
#define UTYPE_TEACHER_STR "1"
#define UTYPE_NORMAL_STR "2"

static_assert(meta::equal(UTYPE_ADMIN_STR,
	meta::ToString<(int)UserType::ADMIN>::value), "Update the above #define");
static_assert(meta::equal(UTYPE_TEACHER_STR,
	meta::ToString<(int)UserType::TEACHER>::value), "Update the above #define");
static_assert(meta::equal(UTYPE_NORMAL_STR,
	meta::ToString<(int)UserType::NORMAL>::value), "Update the above #define");

// Contest's users
enum class ContestUserMode : uint8_t {
	CONTESTANT = 0,
	MODERATOR = 1,
	OWNER = 2
};

#define CU_MODE_CONTESTANT_STR "0"
static_assert(meta::equal(CU_MODE_CONTESTANT_STR,
	meta::ToString<(int)ContestUserMode::CONTESTANT>::value),
	"Update the above #define");

#define CU_MODE_MODERATOR_STR "1"
static_assert(meta::equal(CU_MODE_MODERATOR_STR,
	meta::ToString<(int)ContestUserMode::MODERATOR>::value),
	"Update the above #define");

#define CU_MODE_OWNER_STR "2"
static_assert(meta::equal(CU_MODE_OWNER_STR,
	meta::ToString<(int)ContestUserMode::OWNER>::value),
	"Update the above #define");

// Session
constexpr uint SESSION_ID_LEN = 30;
constexpr uint SESSION_CSRF_TOKEN_LEN = 20;
constexpr uint SESSION_IP_LEN = 15;
constexpr uint TMP_SESSION_MAX_LIFETIME = 60 * 60; // 1 hour [s]
constexpr uint SESSION_MAX_LIFETIME = 30 * 24 * 60 * 60; // 30 days [s]

// Problems
constexpr uint PROBLEM_NAME_MAX_LEN = 128;
constexpr uint PROBLEM_LABEL_MAX_LEN = 64;
constexpr uint NEW_STATEMENT_MAX_SIZE = 10 << 20; // 10 MB

// Problems' tags
constexpr uint PROBLEM_TAG_MAX_LEN = 128;

// Contests
constexpr uint CONTEST_NAME_MAX_LEN = 128;
// Contest rounds
constexpr uint CONTEST_ROUND_NAME_MAX_LEN = 128;
constexpr uint CONTEST_ROUND_DATETIME_LEN = 19;
// Contest problems
constexpr uint CONTEST_PROBLEM_NAME_MAX_LEN =
	meta::max(128, PROBLEM_NAME_MAX_LEN);

// Contest entry tokens
constexpr uint CONTEST_ENTRY_TOKEN_LEN = 48;
constexpr uint CONTEST_ENTRY_SHORT_TOKEN_LEN = 8;
constexpr uint CONTEST_ENTRY_SHORT_TOKEN_MAX_LIFETIME = 60 * 60; // 1 hour [s]

// Files
constexpr uint FILE_ID_LEN = 30;
constexpr uint FILE_NAME_MAX_LEN = 128;
constexpr uint FILE_DESCRIPTION_MAX_LEN = 512;
constexpr uint FILE_MAX_SIZE = 128 << 20; // 128 MB

// Submissions
constexpr uint SOLUTION_MAX_SIZE = 100 << 10; // 100 Kib


enum class ProblemType : uint8_t {
	PUBLIC = 1,
	PRIVATE = 2,
	CONTEST_ONLY = 3,
};

#define PTYPE_PUBLIC_STR "1"
static_assert(meta::equal(PTYPE_PUBLIC_STR,
	meta::ToString<(int)ProblemType::PUBLIC>::value),
	"Update the above #define");

#define PTYPE_PRIVATE_STR "2"
static_assert(meta::equal(PTYPE_PRIVATE_STR,
	meta::ToString<(int)ProblemType::PRIVATE>::value),
	"Update the above #define");

#define PTYPE_CONTEST_ONLY_STR "3"
static_assert(meta::equal(PTYPE_CONTEST_ONLY_STR,
	meta::ToString<(int)ProblemType::CONTEST_ONLY>::value),
	"Update the above #define");

enum class SubmissionType : uint8_t {
	NORMAL = 0,
	IGNORED = 2,
	PROBLEM_SOLUTION = 3,
};

#define STYPE_NORMAL_STR "0"
static_assert(meta::equal(STYPE_NORMAL_STR,
	meta::ToString<(int)SubmissionType::NORMAL>::value),
	"Update the above #define");

#define STYPE_IGNORED_STR "2"
static_assert(meta::equal(STYPE_IGNORED_STR,
	meta::ToString<(int)SubmissionType::IGNORED>::value),
	"Update the above #define");

#define STYPE_PROBLEM_SOLUTION_STR "3"
static_assert(meta::equal(STYPE_PROBLEM_SOLUTION_STR,
	meta::ToString<(int)SubmissionType::PROBLEM_SOLUTION>::value),
	"Update the above #define");

constexpr inline const char* toString(SubmissionType x) {
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
};

constexpr inline const char* toString(SubmissionLanguage x) {
	switch (x) {
	case SubmissionLanguage::C11: return "C11";
	case SubmissionLanguage::CPP11: return "C++11";
	case SubmissionLanguage::CPP14: return "C++14";
	case SubmissionLanguage::PASCAL: return "Pascal";
	}
	return "Unknown";
}

constexpr inline const char* to_extension(SubmissionLanguage x) {
	switch (x) {
	case SubmissionLanguage::C11: return ".c";
	case SubmissionLanguage::CPP11:
	case SubmissionLanguage::CPP14:
		return ".cpp";
	case SubmissionLanguage::PASCAL: return ".pas";
	}
	return "Unknown";
}

constexpr inline const char* to_MIME(SubmissionLanguage x) {
	switch (x) {
	case SubmissionLanguage::C11: return "text/x-csrc";
	case SubmissionLanguage::CPP11:
	case SubmissionLanguage::CPP14:
		return "text/x-c++src";
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
	PENDING                   = 8 + 0,
	// Fatal
	COMPILATION_ERROR         = 8 + 1,
	CHECKER_COMPILATION_ERROR = 8 + 2,
	JUDGE_ERROR               = 8 + 3
};

DECLARE_ENUM_UNARY_OPERATOR(SubmissionStatus, ~)
DECLARE_ENUM_OPERATOR(SubmissionStatus, |)
DECLARE_ENUM_OPERATOR(SubmissionStatus, &)

// Non-fatal statuses
static_assert(meta::max(SubmissionStatus::OK, SubmissionStatus::WA,
		SubmissionStatus::TLE, SubmissionStatus::MLE, SubmissionStatus::RTE) <
	SubmissionStatus::PENDING,
	"Needed as a boundary between non-fatal and fatal statuses - it is strongly"
	" used during selection of the final submission");

// Fatal statuses
static_assert(meta::min(SubmissionStatus::COMPILATION_ERROR,
	SubmissionStatus::CHECKER_COMPILATION_ERROR, SubmissionStatus::JUDGE_ERROR)
	> SubmissionStatus::PENDING,
	"Needed as a boundary between non-fatal and fatal statuses - it is strongly"
	" used during selection of the final submission");

constexpr inline bool is_special(SubmissionStatus status) {
	return (status >= SubmissionStatus::PENDING);
}

constexpr inline bool is_fatal(SubmissionStatus status) {
	return (status > SubmissionStatus::PENDING);
}

inline constexpr const char* css_color_class(SubmissionStatus status) noexcept {
	switch (status) {
	case SubmissionStatus::OK: return "green";
	case SubmissionStatus::WA: return "red";
	case SubmissionStatus::TLE: return "yellow";
	case SubmissionStatus::MLE: return "yellow";
	case SubmissionStatus::RTE: return "intense-red";
	case SubmissionStatus::PENDING: return "";
	case SubmissionStatus::COMPILATION_ERROR: return "purple";
	case SubmissionStatus::CHECKER_COMPILATION_ERROR: return "blue";
	case SubmissionStatus::JUDGE_ERROR: return "blue";
	}

	return ""; // Shouldn't happen
}

#define SSTATUS_PENDING_STR "8"
static_assert(meta::equal(SSTATUS_PENDING_STR,
	meta::ToString<(int)SubmissionStatus::PENDING>::value),
	"Update the above #define");

enum class SubmissionFinalSelectingMethod {
	LAST_COMPILING = 0,
	WITH_HIGHEST_SCORE = 1
};

#define SFSM_LAST_COMPILING "0"
static_assert(meta::equal(SFSM_LAST_COMPILING,
	meta::ToString<(int)SubmissionFinalSelectingMethod::LAST_COMPILING>::value),

#define SFSM_WITH_HIGHEST_SCORE "1"
	"Update the above #define");
static_assert(meta::equal(SFSM_WITH_HIGHEST_SCORE,
	meta::ToString<(int)SubmissionFinalSelectingMethod::WITH_HIGHEST_SCORE>::value),
	"Update the above #define");

enum class JobType : uint8_t {
	JUDGE_SUBMISSION = 1,
	ADD_PROBLEM = 2,
	REUPLOAD_PROBLEM = 3,
	ADD_JUDGE_MODEL_SOLUTION = 4,
	REUPLOAD_JUDGE_MODEL_SOLUTION = 5,
	EDIT_PROBLEM = 6,
	DELETE_PROBLEM = 7,
	CONTEST_PROBLEM_RESELECT_FINAL_SUBMISSIONS = 8,
	DELETE_USER = 9,
	DELETE_CONTEST = 10,
	DELETE_CONTEST_ROUND = 11,
	DELETE_CONTEST_PROBLEM = 12,
	RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION = 13,
	MERGE_PROBLEMS = 14,
	REJUDGE_SUBMISSION = 15,
	DELETE_FILE = 16,
	CHANGE_PROBLEM_STATEMENT = 17,
};

#define JTYPE_JUDGE_SUBMISSION_STR "1"
static_assert(meta::equal(JTYPE_JUDGE_SUBMISSION_STR,
	meta::ToString<(int)JobType::JUDGE_SUBMISSION>::value),
	"Update the above #define");

#define JTYPE_ADD_PROBLEM_STR "2"
static_assert(meta::equal(JTYPE_ADD_PROBLEM_STR,
	meta::ToString<(int)JobType::ADD_PROBLEM>::value),
	"Update the above #define");

#define JTYPE_REUPLOAD_PROBLEM_STR "3"
static_assert(meta::equal(JTYPE_REUPLOAD_PROBLEM_STR,
	meta::ToString<(int)JobType::REUPLOAD_PROBLEM>::value),
	"Update the above #define");

#define JTYPE_ADD_JUDGE_MODEL_SOLUTION_STR "4"
static_assert(meta::equal(JTYPE_ADD_JUDGE_MODEL_SOLUTION_STR,
	meta::ToString<(int)JobType::ADD_JUDGE_MODEL_SOLUTION>::value),
	"Update the above #define");

#define JTYPE_REUPLOAD_JUDGE_MODEL_SOLUTION_STR "5"
static_assert(meta::equal(JTYPE_REUPLOAD_JUDGE_MODEL_SOLUTION_STR,
	meta::ToString<(int)JobType::REUPLOAD_JUDGE_MODEL_SOLUTION>::value),
	"Update the above #define");

#define JTYPE_EDIT_PROBLEM_STR "6"
static_assert(meta::equal(JTYPE_EDIT_PROBLEM_STR,
	meta::ToString<(int)JobType::EDIT_PROBLEM>::value),
	"Update the above #define");

#define JTYPE_DELETE_PROBLEM_STR "7"
static_assert(meta::equal(JTYPE_DELETE_PROBLEM_STR,
	meta::ToString<(int)JobType::DELETE_PROBLEM>::value),
	"Update the above #define");

#define JTYPE_CONTEST_PROBLEM_RESELECT_FINAL_SUBMISSIONS_STR "8"
static_assert(meta::equal(JTYPE_CONTEST_PROBLEM_RESELECT_FINAL_SUBMISSIONS_STR,
	meta::ToString<(int)JobType::CONTEST_PROBLEM_RESELECT_FINAL_SUBMISSIONS>::value),
	"Update the above #define");

#define JTYPE_DELETE_USER_STR "9"
static_assert(meta::equal(JTYPE_DELETE_USER_STR,
	meta::ToString<(int)JobType::DELETE_USER>::value),
	"Update the above #define");

#define JTYPE_DELETE_CONTEST_STR "10"
static_assert(meta::equal(JTYPE_DELETE_CONTEST_STR,
	meta::ToString<(int)JobType::DELETE_CONTEST>::value),
	"Update the above #define");

#define JTYPE_DELETE_CONTEST_ROUND_STR "11"
static_assert(meta::equal(JTYPE_DELETE_CONTEST_ROUND_STR,
	meta::ToString<(int)JobType::DELETE_CONTEST_ROUND>::value),
	"Update the above #define");

#define JTYPE_DELETE_CONTEST_PROBLEM_STR "12"
static_assert(meta::equal(JTYPE_DELETE_CONTEST_PROBLEM_STR,
	meta::ToString<(int)JobType::DELETE_CONTEST_PROBLEM>::value),
	"Update the above #define");

#define JTYPE_RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION_STR "13"
static_assert(meta::equal(JTYPE_RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION_STR,
	meta::ToString<(int)JobType::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION>::value),
	"Update the above #define");

#define JTYPE_MERGE_PROBLEMS_STR "14"
static_assert(meta::equal(JTYPE_MERGE_PROBLEMS_STR,
	meta::ToString<(int)JobType::MERGE_PROBLEMS>::value),
	"Update the above #define");

#define JTYPE_REJUDGE_SUBMISSION_STR "15"
static_assert(meta::equal(JTYPE_REJUDGE_SUBMISSION_STR,
	meta::ToString<(int)JobType::REJUDGE_SUBMISSION>::value),
	"Update the above #define");

#define JTYPE_DELETE_FILE_STR "16"
static_assert(meta::equal(JTYPE_DELETE_FILE_STR,
	meta::ToString<(int)JobType::DELETE_FILE>::value),
	"Update the above #define");

#define JTYPE_CHANGE_PROBLEM_STATEMENT_STR "17"
static_assert(meta::equal(JTYPE_CHANGE_PROBLEM_STATEMENT_STR,
	meta::ToString<(int)JobType::CHANGE_PROBLEM_STATEMENT>::value),
	"Update the above #define");

constexpr inline const char* toString(JobType x) {
	using JT = JobType;
	switch (x) {
	case JT::JUDGE_SUBMISSION: return "JUDGE_SUBMISSION";
	case JT::ADD_PROBLEM: return "ADD_PROBLEM";
	case JT::REUPLOAD_PROBLEM: return "REUPLOAD_PROBLEM";
	case JT::ADD_JUDGE_MODEL_SOLUTION: return "ADD_JUDGE_MODEL_SOLUTION";
	case JT::REUPLOAD_JUDGE_MODEL_SOLUTION:
		return "REUPLOAD_JUDGE_MODEL_SOLUTION";
	case JT::EDIT_PROBLEM: return "EDIT_PROBLEM";
	case JT::DELETE_PROBLEM: return "DELETE_PROBLEM";
	case JT::CONTEST_PROBLEM_RESELECT_FINAL_SUBMISSIONS:
		return "CONTEST_PROBLEM_RESELECT_FINAL_SUBMISSIONS";
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
	}
	return "Unknown";
}

constexpr inline bool is_problem_job(JobType x) {
	using JT = JobType;
	switch (x) {
	case JT::ADD_PROBLEM: return true;
	case JT::ADD_JUDGE_MODEL_SOLUTION: return true;
	case JT::REUPLOAD_PROBLEM: return true;
	case JT::REUPLOAD_JUDGE_MODEL_SOLUTION: return true;
	case JT::EDIT_PROBLEM: return true;
	case JT::DELETE_PROBLEM: return true;
	case JT::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION: return true;
	case JT::MERGE_PROBLEMS: return true;
	case JT::CHANGE_PROBLEM_STATEMENT: return true;
	case JT::JUDGE_SUBMISSION: return false;
	case JT::REJUDGE_SUBMISSION: return false;
	case JT::CONTEST_PROBLEM_RESELECT_FINAL_SUBMISSIONS: return false;
	case JT::DELETE_USER: return false;
	case JT::DELETE_CONTEST: return false;
	case JT::DELETE_CONTEST_ROUND: return false;
	case JT::DELETE_CONTEST_PROBLEM: return false;
	case JT::DELETE_FILE: return false;
	}
	return false;
}

constexpr inline bool is_submission_job(JobType x) {
	using JT = JobType;
	switch (x) {
	case JT::JUDGE_SUBMISSION: return true;
	case JT::REJUDGE_SUBMISSION: return true;
	case JT::ADD_PROBLEM: return false;
	case JT::ADD_JUDGE_MODEL_SOLUTION: return false;
	case JT::REUPLOAD_PROBLEM: return false;
	case JT::REUPLOAD_JUDGE_MODEL_SOLUTION: return false;
	case JT::EDIT_PROBLEM: return false;
	case JT::DELETE_PROBLEM: return false;
	case JT::CONTEST_PROBLEM_RESELECT_FINAL_SUBMISSIONS: return false;
	case JT::DELETE_USER: return false;
	case JT::DELETE_CONTEST: return false;
	case JT::DELETE_CONTEST_ROUND: return false;
	case JT::DELETE_CONTEST_PROBLEM: return false;
	case JT::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION: return false;
	case JT::MERGE_PROBLEMS: return false;
	case JT::DELETE_FILE: return false;
	case JT::CHANGE_PROBLEM_STATEMENT: return false;
	}
	return false;
}

// The greater, the more important
constexpr inline uint priority(JobType x) {
	using JT = JobType;
	switch (x) {
	case JT::DELETE_FILE: return 40;
	case JT::DELETE_PROBLEM: return 30;
	case JT::CONTEST_PROBLEM_RESELECT_FINAL_SUBMISSIONS: return 30;
	case JT::DELETE_USER: return 30;
	case JT::DELETE_CONTEST: return 30;
	case JT::DELETE_CONTEST_ROUND: return 30;
	case JT::DELETE_CONTEST_PROBLEM: return 30;
	case JT::MERGE_PROBLEMS: return 25;
	case JT::EDIT_PROBLEM: return 20;
	case JT::CHANGE_PROBLEM_STATEMENT: return 20;
	case JT::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION: return 20;
	case JT::ADD_JUDGE_MODEL_SOLUTION: return 15;
	case JT::REUPLOAD_JUDGE_MODEL_SOLUTION: return 15;
	case JT::ADD_PROBLEM: return 10;
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

#define JSTATUS_PENDING_STR "1"
static_assert(meta::equal(JSTATUS_PENDING_STR,
	meta::ToString<(int)JobStatus::PENDING>::value),
	"Update the above #define");

#define JSTATUS_NOTICED_PENDING_STR "2"
static_assert(meta::equal(JSTATUS_NOTICED_PENDING_STR,
	meta::ToString<(int)JobStatus::NOTICED_PENDING>::value),
	"Update the above #define");

#define JSTATUS_IN_PROGRESS_STR "3"
static_assert(meta::equal(JSTATUS_IN_PROGRESS_STR,
	meta::ToString<(int)JobStatus::IN_PROGRESS>::value),
	"Update the above #define");

#define JSTATUS_DONE_STR "4"
static_assert(meta::equal(JSTATUS_DONE_STR,
	meta::ToString<(int)JobStatus::DONE>::value), "Update the above #define");

#define JSTATUS_FAILED_STR "5"
static_assert(meta::equal(JSTATUS_FAILED_STR,
	meta::ToString<(int)JobStatus::FAILED>::value), "Update the above #define");

#define JSTATUS_CANCELED_STR "6"
static_assert(meta::equal(JSTATUS_CANCELED_STR,
	meta::ToString<(int)JobStatus::CANCELED>::value),
	"Update the above #define");

constexpr inline const char* toString(JobStatus x) {
	switch (x) {
	case JobStatus::PENDING: return "Pending";
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

template<class T>
auto internal_file_path(T file_id) { return concat<64>(INTERNAL_FILES_DIR, file_id); }

// Jobs
constexpr uint JOB_LOG_VIEW_MAX_LENGTH = 128 << 10; // 128 KB

// Logs
constexpr const char SERVER_LOG[] = "logs/server.log";
constexpr const char SERVER_ERROR_LOG[] = "logs/server-error.log";
constexpr const char JOB_SERVER_LOG[] = "logs/job-server.log";
constexpr const char JOB_SERVER_ERROR_LOG[] = "logs/job-server-error.log";
// Logs API
constexpr uint LOGS_FIRST_CHUNK_MAX_LEN = 8 << 10; // 8 kB
constexpr uint LOGS_OTHER_CHUNK_MAX_LEN = 64 << 10; // 64 kB

// API
constexpr uint API_FIRST_QUERY_ROWS_LIMIT = 50;
constexpr uint API_OTHER_QUERY_ROWS_LIMIT = 200;

// Job-server notifying file
constexpr const char JOB_SERVER_NOTIFYING_FILE[] = ".job-server.notify";

constexpr uint COMPILATION_ERRORS_MAX_LENGTH = 16 << 10; // 32 KB
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
