#pragma once

// User
constexpr unsigned USERNAME_MAX_LEN = 30;
constexpr unsigned USER_FIRST_NAME_MAX_LEN = 60;
constexpr unsigned USER_LAST_NAME_MAX_LEN = 60;
constexpr unsigned USER_EMAIL_MAX_LEN = 60;
constexpr unsigned SALT_LEN = 64;
constexpr unsigned PASSWORD_HASH_LEN = 128;

// user_type - strongly used -> do not change!
constexpr unsigned UTYPE_ADMIN = 0;
constexpr unsigned UTYPE_TEACHER = 1;
constexpr unsigned UTYPE_NORMAL = 2;

// Session
constexpr unsigned SESSION_ID_LEN = 30;
constexpr unsigned SESSION_IP_LEN = 15;
constexpr unsigned SESSION_MAX_LIFETIME = 7 * 24 * 60 * 60; // 7 days [s]

// Problems
constexpr unsigned PROBLEM_NAME_MAX_LEN = 128;
constexpr unsigned PROBLEM_TAG_LEN = 4;

// Rounds
constexpr unsigned ROUND_NAME_MAX_LEN = 128;

// Files
constexpr unsigned FILE_ID_LEN = 30;
constexpr unsigned FILE_NAME_MAX_LEN = 128;
constexpr unsigned FILE_DESCRIPTION_MAX_LEN = 512;

// Submissions
constexpr unsigned SOLUTION_MAX_SIZE = 100 << 10; // 100 Kib

// Logs
constexpr const char* SERVER_LOG = "logs/server.log";
constexpr const char* SERVER_ERROR_LOG = "logs/server-error.log";
constexpr const char* JUDGE_LOG = "logs/judge.log";
constexpr const char* JUDGE_ERROR_LOG = "logs/judge-error.log";
