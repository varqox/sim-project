#pragma once

#include "job_handler.hh"

namespace job_server::job_handlers {

class DeleteContest final : public JobHandler {
    uint64_t contest_id_;

public:
    DeleteContest(uint64_t job_id, uint64_t contest_id)
    : JobHandler(job_id)
    , contest_id_(contest_id) {}

    void run(sim::mysql::Connection& mysql) final;
};

} // namespace job_server::job_handlers
