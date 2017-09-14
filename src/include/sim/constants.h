#pragma once

#include <cstdint>
#include <simlib/meta.h>

#define SQLITE_DB_FILE "sqlite-sim.db"

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
enum class ContestUserMode : int8_t {
	IS_NULL = -1,
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

// Problems' tags
constexpr uint PROBLEM_TAG_MAX_LEN = 128;

// Contests
constexpr uint CONTEST_NAME_MAX_LEN = 128;
// Contest rounds
constexpr uint CONTEST_ROUND_NAME_MAX_LEN = 128;
// Contest problems
constexpr uint CONTEST_PROBLEM_NAME_MAX_LEN =
	meta::max(128, PROBLEM_NAME_MAX_LEN);

// Files
constexpr uint FILE_ID_LEN = 30;
constexpr uint FILE_NAME_MAX_LEN = 128;
constexpr uint FILE_DESCRIPTION_MAX_LEN = 512;

// Submissions
constexpr uint SOLUTION_MAX_SIZE = 100 << 10; // 100 Kib


enum class ProblemType : uint8_t {
	VOID = 0,
	PUBLIC = 1,
	PRIVATE = 2,
	CONTEST_ONLY = 3,
};

#define PTYPE_VOID_STR "0"
static_assert(meta::equal(PTYPE_VOID_STR,
	meta::ToString<(int)ProblemType::VOID>::value),
	"Update the above #define");

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
	FINAL = 1,
	VOID = 2,
	IGNORED = 3,
	PROBLEM_SOLUTION = 4,
};

#define STYPE_NORMAL_STR "0"
static_assert(meta::equal(STYPE_NORMAL_STR,
	meta::ToString<(int)SubmissionType::NORMAL>::value),
	"Update the above #define");

#define STYPE_FINAL_STR "1"
static_assert(meta::equal(STYPE_FINAL_STR,
	meta::ToString<(int)SubmissionType::FINAL>::value),
	"Update the above #define");

#define STYPE_VOID_STR "2"
static_assert(meta::equal(STYPE_VOID_STR,
	meta::ToString<(int)SubmissionType::VOID>::value),
	"Update the above #define");

#define STYPE_IGNORED_STR "3"
static_assert(meta::equal(STYPE_IGNORED_STR,
	meta::ToString<(int)SubmissionType::IGNORED>::value),
	"Update the above #define");

#define STYPE_PROBLEM_SOLUTION_STR "4"
static_assert(meta::equal(STYPE_PROBLEM_SOLUTION_STR,
	meta::ToString<(int)SubmissionType::PROBLEM_SOLUTION>::value),
	"Update the above #define");

constexpr inline const char* toString(SubmissionType x) {
	switch (x) {
	case SubmissionType::NORMAL: return "Normal";
	case SubmissionType::FINAL: return "Final";
	case SubmissionType::IGNORED: return "Ignored";
	case SubmissionType::PROBLEM_SOLUTION: return "Problem solution";
	case SubmissionType::VOID: return "Void";
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
	FINAL_MASK = 7,
	// Initial
	INITIAL_OK  = OK << 3,
	INITIAL_WA  = WA << 3,
	INITIAL_TLE = TLE << 3,
	INITIAL_MLE = MLE << 3,
	INITIAL_RTE = RTE << 3,
	INITIAL_MASK = FINAL_MASK << 3,
	// Special
	PENDING                   = (8 << 3) + 0,
	// Fatal
	COMPILATION_ERROR         = (8 << 3) + 1,
	CHECKER_COMPILATION_ERROR = (8 << 3) + 2,
	JUDGE_ERROR               = (8 << 3) + 3
};

DECLARE_ENUM_UNARY_OPERATOR(SubmissionStatus, ~)
DECLARE_ENUM_OPERATOR(SubmissionStatus, |)
DECLARE_ENUM_OPERATOR(SubmissionStatus, &)

// Non-fatal statuses
static_assert(meta::max(SubmissionStatus::OK, SubmissionStatus::WA,
	SubmissionStatus::TLE, SubmissionStatus::MLE, SubmissionStatus::RTE,
	SubmissionStatus::INITIAL_OK, SubmissionStatus::INITIAL_WA,
	SubmissionStatus::INITIAL_TLE, SubmissionStatus::INITIAL_MLE,
	SubmissionStatus::INITIAL_RTE) < SubmissionStatus::PENDING,
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

#define SSTATUS_PENDING_STR "64"
static_assert(meta::equal(SSTATUS_PENDING_STR,
	meta::ToString<(int)SubmissionStatus::PENDING>::value),
	"Update the above #define");

enum class JobType : uint8_t {
	VOID = 0,
	JUDGE_SUBMISSION = 1,
	ADD_PROBLEM = 2,
	REUPLOAD_PROBLEM = 3,
	ADD_JUDGE_MODEL_SOLUTION = 4,
	REUPLOAD_JUDGE_MODEL_SOLUTION = 5,
	EDIT_PROBLEM = 6,
	DELETE_PROBLEM = 7,
};

#define JTYPE_VOID_STR "0"
static_assert(meta::equal(JTYPE_VOID_STR,
	meta::ToString<(int)JobType::VOID>::value),
	"Update the above #define");

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

constexpr inline const char* toString(JobType x) {
	using JT = JobType;
	switch (x) {
	case JT::VOID: return "VOID";
	case JT::JUDGE_SUBMISSION: return "JUDGE_SUBMISSION";
	case JT::ADD_PROBLEM: return "ADD_PROBLEM";
	case JT::REUPLOAD_PROBLEM: return "REUPLOAD_PROBLEM";
	case JT::ADD_JUDGE_MODEL_SOLUTION: return "ADD_JUDGE_MODEL_SOLUTION";
	case JT::REUPLOAD_JUDGE_MODEL_SOLUTION:
		return "REUPLOAD_JUDGE_MODEL_SOLUTION";
	case JT::EDIT_PROBLEM: return "EDIT_PROBLEM";
	case JT::DELETE_PROBLEM: return "DELETE_PROBLEM";
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
	case JT::JUDGE_SUBMISSION: return false;
	case JT::VOID: return false;
	}
	return false;
}

constexpr inline bool is_submission_job(JobType x) {
	using JT = JobType;
	switch (x) {
	case JT::JUDGE_SUBMISSION: return true;
	case JT::ADD_PROBLEM: return false;
	case JT::ADD_JUDGE_MODEL_SOLUTION: return false;
	case JT::REUPLOAD_PROBLEM: return false;
	case JT::REUPLOAD_JUDGE_MODEL_SOLUTION: return false;
	case JT::EDIT_PROBLEM: return false;
	case JT::DELETE_PROBLEM: return false;
	case JT::VOID: return false;
	}
	return false;
}

// The greater, the more important
constexpr inline uint priority(JobType x) {
	using JT = JobType;
	switch (x) {
	case JT::EDIT_PROBLEM: return 20;
	case JT::DELETE_PROBLEM: return 20;
	case JT::ADD_JUDGE_MODEL_SOLUTION: return 15;
	case JT::REUPLOAD_JUDGE_MODEL_SOLUTION: return 15;
	case JT::ADD_PROBLEM: return 10;
	case JT::REUPLOAD_PROBLEM: return 10;
	case JT::JUDGE_SUBMISSION: return 5;
	case JT::VOID: return 0;
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
	case JobStatus::CANCELED: return "Cancelled";
	}
	return "Unknown";
}

// Jobs
constexpr uint REPORT_PREVIEW_MAX_LENGTH = 16 << 10; // 16 KB

// Logs
constexpr const char SERVER_LOG[] = "logs/server.log";
constexpr const char SERVER_ERROR_LOG[] = "logs/server-error.log";
constexpr const char JOB_SERVER_LOG[] = "logs/job-server.log";
constexpr const char JOB_SERVER_ERROR_LOG[] = "logs/job-server-error.log";

// Job-server notifying file
constexpr const char JOB_SERVER_NOTIFYING_FILE[] = ".job-server.notify";

constexpr uint COMPILATION_ERRORS_MAX_LENGTH = 16 << 10; // 32 KB
constexpr timespec CHECKER_COMPILATION_TIME_LIMIT = {30, 0}; // 30 s
constexpr timespec SOLUTION_COMPILATION_TIME_LIMIT = {30, 0}; // 30 s
constexpr const char PROOT_PATH[] = "./proot";
