#pragma once

#include "job_handler.hh"

#include <sim/jobs/utils.hh>
#include <utility>

namespace job_server::job_handlers {

class ChangeProblemStatement final : public JobHandler {
    uint64_t problem_id_;
    uint64_t job_file_id_;
    sim::jobs::ChangeProblemStatementInfo info_;

public:
    ChangeProblemStatement(
        uint64_t job_id,
        uint64_t problem_id,
        uint64_t job_file_id,
        sim::jobs::ChangeProblemStatementInfo info
    )
    : JobHandler(job_id)
    , problem_id_(problem_id)
    , job_file_id_(job_file_id)
    , info_(std::move(info)) {}

    void run() final;
};

} // namespace job_server::job_handlers
