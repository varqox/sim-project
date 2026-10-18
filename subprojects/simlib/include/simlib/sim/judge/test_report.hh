#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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

    struct Communication {
        struct Instance {
            uint32_t id;
            Status status;
            Program program;
            uint64_t output_size;
            bool cancelled;
        };

        std::vector<Instance> instances;
        std::chrono::microseconds total_cpu_time{};
        std::chrono::nanoseconds elapsed{};
        bool complete = false;
    };

    std::optional<Communication> communication = std::nullopt;
};

} // namespace sim::judge
