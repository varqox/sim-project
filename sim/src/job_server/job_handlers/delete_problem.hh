#pragma once

#include "src/job_server/job_handlers/job_handler.hh"

namespace job_server::job_handlers {

class DeleteProblem final : public JobHandler {
    uint64_t problem_id_;

public:
    DeleteProblem(uint64_t job_id, uint64_t problem_id)
    : JobHandler(job_id)
    , problem_id_(problem_id) {}

    void run() final;
};

} // namespace job_server::job_handlers
