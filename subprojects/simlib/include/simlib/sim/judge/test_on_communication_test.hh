#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <simlib/file_path.hh>
#include <simlib/sim/communication_config.hh>
#include <simlib/sim/judge/language_suite/suite.hh>
#include <simlib/sim/judge/test_report.hh>

namespace sim::judge {

struct CommunicationTestArgs {
    language_suite::Suite& compiled_program; // NOLINT
    language_suite::Suite& compiled_manager; // NOLINT
    FilePath test_input;
    uint32_t instances;
    std::optional<uint32_t> min_instances = std::nullopt;
    uint32_t max_concurrent;

    struct Program {
        std::chrono::nanoseconds time_limit;
        std::chrono::nanoseconds cpu_time_limit;
        uint64_t memory_limit_in_bytes;
        uint64_t output_size_limit_in_bytes = 64 << 20;
    } program;

    struct Manager {
        std::chrono::nanoseconds time_limit;
        std::chrono::nanoseconds cpu_time_limit;
        uint64_t memory_limit_in_bytes;
        uint64_t max_comment_len;
    } manager;

    struct Limits {
        uint64_t input_size_in_bytes = 64 << 20;
        uint64_t session_data_in_bytes = 256 << 20;
        uint64_t memory_in_bytes = CommunicationConfig::default_session_memory_in_bytes;
        std::chrono::nanoseconds startup_time = std::chrono::seconds{5};
    } limits = {};
};

TestReport test_on_communication_test(CommunicationTestArgs args);

} // namespace sim::judge
