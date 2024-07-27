#pragma once

#include "../common.hh"

#include <cstdint>
#include <sim/internal_files/internal_file.hh>
#include <sim/mysql/mysql.hh>
#include <sim/problems/problem.hh>
#include <simlib/file_remover.hh>
#include <simlib/inplace_buff.hh>
#include <simlib/libzip.hh>
#include <simlib/sim/simfile.hh>

namespace job_server::job_handlers::add_or_reupload_problem {

struct ConstructSimfileOptions {
    const std::string& package_path; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    std::string name;
    std::string label;
    std::optional<uint64_t> memory_limit_in_mib;
    std::optional<uint64_t> fixed_time_limit_in_ns;
    bool force_time_limits_reset;
    bool ignore_simfile;
    bool look_for_new_tests;
    bool reset_scoring;
};

// Returns std::nullopt on error
std::optional<sim::Simfile> construct_simfile(Logger& logger, ConstructSimfileOptions&& options);

struct CreatePackageWithSimfileRes {
    decltype(sim::internal_files::InternalFile::id) new_package_file_id;
    std::string new_package_main_dir;
    ZipFile new_package_zip;
    FileRemover new_package_file_remover;
};

std::optional<CreatePackageWithSimfileRes> create_package_with_simfile(
    sim::mysql::Connection& mysql,
    Logger& logger,
    const std::string& input_package_path,
    const sim::Simfile& simfile,
    const std::string& current_datetime
);

// Returns solution file removers
std::vector<FileRemover> submit_solutions(
    sim::mysql::Connection& mysql,
    Logger& logger,
    const sim::Simfile& simfile,
    ZipFile& package_zip,
    const std::string& package_main_dir,
    decltype(sim::problems::Problem::id) problem_id,
    const std::string& current_datetime
);

} // namespace job_server::job_handlers::add_or_reupload_problem
