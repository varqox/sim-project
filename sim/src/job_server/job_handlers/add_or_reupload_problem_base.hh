#pragma once

#include "sim/jobs/job.hh"
#include "sim/jobs/utils.hh"
#include "simlib/libzip.hh"
#include "src/job_server/job_handlers/job_handler.hh"

#include <utility>

namespace job_server::job_handlers {

class AddOrReuploadProblemBase : virtual public JobHandler {
protected:
    decltype(sim::jobs::Job::type) job_type_;
    StringView job_creator_;
    sim::jobs::AddProblemInfo info_;
    FileRemover package_file_remover_;
    uint64_t job_file_id_;
    std::optional<uint64_t> tmp_file_id_;
    std::optional<uint64_t> problem_id_;
    // Internal state
    bool need_main_solution_judge_report_ = false;

private:
    bool replace_db_job_log_ = false;
    ZipFile zip_;
    std::string main_dir_;
    std::string current_date_;
    std::string simfile_str_;
    sim::Simfile simfile_;

    void load_job_log_from_db();

protected:
    AddOrReuploadProblemBase(decltype(sim::jobs::Job::type) job_type, StringView job_creator,
            sim::jobs::AddProblemInfo info, uint64_t job_file_id,
            std::optional<uint64_t> tmp_file_id, std::optional<uint64_t> problem_id)
    : job_type_(job_type)
    , job_creator_(job_creator)
    , info_(std::move(info))
    , job_file_id_(job_file_id)
    , tmp_file_id_(tmp_file_id)
    , problem_id_(problem_id) {
        if (tmp_file_id.has_value()) {
            load_job_log_from_db();
        }
    }

    static void assert_transaction_is_open();

    // Runs conver and places package into a temporary internal file. Sets
    // need_main_solution_judge_report
    void build_package();

private:
    // Initializes internal variables for use by the next functions
    void open_package();

protected:
    void add_problem_to_db();

    void replace_problem_in_db();

    void submit_solutions();

    using JobHandler::job_done;

    void job_done(bool& job_was_canceled);
};

} // namespace job_server::job_handlers
