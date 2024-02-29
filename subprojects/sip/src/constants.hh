#pragma once

#include <chrono>
#include <cstdint>

using uint = unsigned;

constexpr uint COMPILATION_ERRORS_MAX_LENGTH = 16 << 10; // 32 KiB

constexpr std::chrono::nanoseconds SOLUTION_COMPILATION_TIME_LIMIT = std::chrono::seconds(30);
constexpr uint64_t SOLUTION_COMPILATION_MEMORY_LIMIT = 1 << 30;
constexpr std::chrono::nanoseconds CHECKER_COMPILATION_TIME_LIMIT = std::chrono::seconds(30);
constexpr uint64_t CHECKER_COMPILATION_MEMORY_LIMIT = 1 << 30;
constexpr std::chrono::nanoseconds GENERATOR_COMPILATION_TIME_LIMIT = std::chrono::seconds(30);
constexpr uint64_t GENERATOR_COMPILATION_MEMORY_LIMIT = 1 << 30;
// Conver::ResetTimeLimitsOptions and Conver::Options
constexpr std::chrono::nanoseconds MIN_TIME_LIMIT = std::chrono::milliseconds(100);
constexpr std::chrono::nanoseconds MAX_TIME_LIMIT = std::chrono::seconds(22);
constexpr double SOLUTION_RUNTIME_COEFFICIENT = 3;
// JudgeWorker
constexpr std::chrono::nanoseconds CHECKER_TIME_LIMIT = std::chrono::seconds(22);
constexpr uint64_t CHECKER_MEMORY_LIMIT = 512 << 20; // 512
constexpr double SCORE_CUT_LAMBDA = 2. / 3.; // See JudgeWorker::score_cut_lambda

constexpr auto GENERATOR_TIME_LIMIT = std::chrono::seconds{100};
constexpr uint64_t GENERATOR_MEMORY_LIMIT = 4ULL << 30;
constexpr uint64_t GENERATED_INPUT_FILE_SIZE_LIMIT = 1 << 30;

constexpr auto LATEX_COMPILATION_TIME_LIMIT = std::chrono::seconds{100};
