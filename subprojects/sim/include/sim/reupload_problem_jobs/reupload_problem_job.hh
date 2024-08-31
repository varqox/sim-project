#pragma once

#include <cstdint>
#include <optional>
#include <sim/jobs/job.hh>
#include <sim/problems/problem.hh>

namespace sim::reupload_problem_jobs {

struct ReuploadProblemJob {
    decltype(jobs::Job::id) id;
    decltype(internal_files::InternalFile::id) file_id;
    bool force_time_limits_reset;
    bool ignore_simfile;
    decltype(problems::Problem::name) name;
    decltype(problems::Problem::label) label;
    std::optional<uint64_t> memory_limit_in_mib;
    std::optional<uint64_t> fixed_time_limit_in_ns;
    bool reset_scoring;
    bool look_for_new_tests;
    static constexpr size_t COLUMNS_NUM = 10;
};

} // namespace sim::reupload_problem_jobs
