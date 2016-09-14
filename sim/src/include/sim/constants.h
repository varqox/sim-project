#pragma once

#include <cstdint>
#include <simlib/meta.h>

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
constexpr uint UTYPE_ADMIN = 0;
constexpr uint UTYPE_TEACHER = 1;
constexpr uint UTYPE_NORMAL = 2;

#define UTYPE_ADMIN_STR "0"
#define UTYPE_TEACHER_STR "1"
#define UTYPE_NORMAL_STR "2"

static_assert(meta::equal(UTYPE_ADMIN_STR,
	meta::ToString<UTYPE_ADMIN>::value), "Update the above #define");
static_assert(meta::equal(UTYPE_TEACHER_STR,
	meta::ToString<UTYPE_TEACHER>::value), "Update the above #define");
static_assert(meta::equal(UTYPE_NORMAL_STR,
	meta::ToString<UTYPE_NORMAL>::value), "Update the above #define");

constexpr uint CU_MODE_CONTESTANT = 0;
constexpr uint CU_MODE_MODERATOR = 1;

#define CU_MODE_CONTESTANT_STR "0"
#define CU_MODE_MODERATOR_STR "1"

static_assert(meta::equal(CU_MODE_CONTESTANT_STR,
	meta::ToString<CU_MODE_CONTESTANT>::value), "Update the above #define");
static_assert(meta::equal(CU_MODE_MODERATOR_STR,
	meta::ToString<CU_MODE_MODERATOR>::value), "Update the above #define");

// Session
constexpr uint SESSION_ID_LEN = 30;
constexpr uint SESSION_CSRF_TOKEN_LEN = 20;
constexpr uint SESSION_IP_LEN = 15;
constexpr uint SESSION_MAX_LIFETIME = 7 * 24 * 60 * 60; // 7 days [s]

// Problems
constexpr uint PROBLEM_NAME_MAX_LEN = 128;
constexpr uint PROBLEM_TAG_MAX_LEN = 64;

// Rounds
constexpr uint ROUND_NAME_MAX_LEN = 128;

// Files
constexpr uint FILE_ID_LEN = 30;
constexpr uint FILE_NAME_MAX_LEN = 128;
constexpr uint FILE_DESCRIPTION_MAX_LEN = 512;

// Submissions
constexpr uint SOLUTION_MAX_SIZE = 100 << 10; // 100 Kib
constexpr uint SUBMISSIONS_ON_USER_PROFILE_LIMIT = 16;

// Initial and final values may be combined, but not special
enum class SubmissionStatus : uint8_t {
	VOID = 0,
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
	WAITING                   = (8 << 3) + 0,
	COMPILATION_ERROR         = (8 << 3) + 1,
	CHECKER_COMPILATION_ERROR = (8 << 3) + 2,
	JUDGE_ERROR               = (8 << 3) + 3,
};

#define SSTATUS_WAITING_STR "64"
static_assert(meta::equal(SSTATUS_WAITING_STR,
	meta::ToString<(int)SubmissionStatus::WAITING>::value),
	"Update the above #define");

// Non-fatal statuses
static_assert(meta::max(SubmissionStatus::OK, SubmissionStatus::WA,
	SubmissionStatus::TLE, SubmissionStatus::MLE, SubmissionStatus::RTE,
	SubmissionStatus::INITIAL_OK, SubmissionStatus::INITIAL_WA,
	SubmissionStatus::INITIAL_TLE, SubmissionStatus::INITIAL_MLE,
	SubmissionStatus::INITIAL_RTE) < SubmissionStatus::WAITING,
	"Needed as a boundary between non-fatal and fatal statuses");

// Fatal statuses
static_assert(meta::min(SubmissionStatus::COMPILATION_ERROR,
	SubmissionStatus::CHECKER_COMPILATION_ERROR, SubmissionStatus::JUDGE_ERROR)
	> SubmissionStatus::WAITING,
	"Needed as a boundary between non-fatal and fatal statuses");

inline SubmissionStatus operator |(SubmissionStatus a, SubmissionStatus b) {
	return static_cast<SubmissionStatus>(
		static_cast<int>(a) | static_cast<int>(b));
}

inline SubmissionStatus operator &(SubmissionStatus a, SubmissionStatus b) {
	return static_cast<SubmissionStatus>(
		static_cast<int>(a) & static_cast<int>(b));
}

enum class SubmissionType : uint8_t {
	NORMAL = 0,
	FINAL = 1,
	IGNORED = 2
};


#define STYPE_NORMAL_STR "0"
static_assert(meta::equal(STYPE_NORMAL_STR,
	meta::ToString<(int)SubmissionType::NORMAL>::value),
	"Update the above #define");
#define STYPE_FINAL_STR "1"
static_assert(meta::equal(STYPE_FINAL_STR,
	meta::ToString<(int)SubmissionType::FINAL>::value),
	"Update the above #define");
#define STYPE_IGNORED_STR "2"
static_assert(meta::equal(STYPE_IGNORED_STR,
	meta::ToString<(int)SubmissionType::IGNORED>::value),
	"Update the above #define");

#if __cplusplus > 201103L
# warning "Make the below function constexpr"
#endif
inline const char* toString(SubmissionType x) {
	switch (x) {
	case SubmissionType::NORMAL: return "Normal";
	case SubmissionType::FINAL: return "Final";
	case SubmissionType::IGNORED: return "Ignored";
	}
	return "Unknown";
}

// Logs
constexpr const char* SERVER_LOG = "logs/server.log";
constexpr const char* SERVER_ERROR_LOG = "logs/server-error.log";
constexpr const char* JUDGE_LOG = "logs/judge.log";
constexpr const char* JUDGE_ERROR_LOG = "logs/judge-error.log";

constexpr uint COMPILATION_ERRORS_MAX_LENGTH = 16 << 10; // 32 KB
constexpr uint64_t CHECKER_COMPILATION_TIME_LIMIT = 30e6; // 30 s
constexpr uint64_t SOLUTION_COMPILATION_TIME_LIMIT = 30e6; // 30 s
constexpr const char* PROOT_PATH = "./proot";
