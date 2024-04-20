#pragma once

#include "job_handler.hh"

namespace job_server::job_handlers {

class DeleteUser final : public JobHandler {
    uint64_t user_id_;

public:
    DeleteUser(uint64_t job_id, uint64_t user_id) : JobHandler(job_id), user_id_(user_id) {}

    void run(sim::mysql::Connection& mysql) final;
};

} // namespace job_server::job_handlers
