#pragma once

#include <simlib/file_contents.hh>
#include <simlib/sim/judge/language_suite/suite.hh>
#include <simlib/temporary_file.hh>

inline void compile(sim::judge::language_suite::Suite& suite, StringView source) {
    auto source_file = TemporaryFile{"/tmp/sim_judge_test_compile.XXXXXX"};
    put_file_contents(source_file.path(), source);
    suite
        .compile(
            source_file.path(),
            {
                .time_limit = std::chrono::seconds{10},
                .cpu_time_limit = std::chrono::seconds{10},
                .memory_limit_in_bytes = 1 << 30,
                .max_file_size_in_bytes = 10 << 20,
            }
        )
        .unwrap();
}
