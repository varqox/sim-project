#pragma once

#include "job_handler.hh"

namespace job_server::job_handlers {

class MergeProblems final : public JobHandler {
public:
    explicit MergeProblems(uint64_t job_id) : JobHandler(job_id) {}

    void run(sim::mysql::Connection& mysql) final;

private:
    void run_impl(sim::mysql::Connection& mysql);
};

} // namespace job_server::job_handlers
