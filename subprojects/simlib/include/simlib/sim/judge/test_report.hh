#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace sim::judge {

struct TestReport {
    // Sorted by decreasing priority
    enum class Status : uint8_t {
        TimeLimitExceeded,
        MemoryLimitExceeded,
        OutputSizeLimitExceeded,
        RuntimeError,
        CheckerError,
        WrongAnswer,
        OK,
    } status;
    std::string comment;
    double score; // in range [0, 1]

    struct Program {
        std::chrono::nanoseconds runtime;
        std::chrono::microseconds cpu_time;
        uint64_t peak_memory_in_bytes;
    } program;

    struct Checker {
        std::chrono::nanoseconds runtime;
        std::chrono::microseconds cpu_time;
        uint64_t peak_memory_in_bytes;
    };

    std::optional<Checker> checker;
};

} // namespace sim::judge
