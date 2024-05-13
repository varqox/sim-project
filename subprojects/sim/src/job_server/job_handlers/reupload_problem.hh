#pragma once

#include "job_handler.hh"

#include <sim/jobs/old_job.hh>

namespace job_server::job_handlers {

class ReuploadProblem final : public JobHandler {
public:
    explicit ReuploadProblem(uint64_t job_id) : JobHandler(job_id) {}

    void run(sim::mysql::Connection& mysql) override;
};

} // namespace job_server::job_handlers
