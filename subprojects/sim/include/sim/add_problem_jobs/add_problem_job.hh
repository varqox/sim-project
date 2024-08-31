#pragma once

#include <cstdint>
#include <optional>
#include <sim/internal_files/internal_file.hh>
#include <sim/jobs/job.hh>
#include <sim/problems/problem.hh>

namespace sim::add_problem_jobs {

struct AddProblemJob {
    decltype(jobs::Job::id) id;
    decltype(internal_files::InternalFile::id) file_id;
    decltype(problems::Problem::visibility) visibility;
    bool force_time_limits_reset;
    bool ignore_simfile;
    decltype(problems::Problem::name) name;
    decltype(problems::Problem::label) label;
    std::optional<uint64_t> memory_limit_in_mib;
    std::optional<uint64_t> fixed_time_limit_in_ns;
    bool reset_scoring;
    bool look_for_new_tests;
};

} // namespace sim::add_problem_jobs
