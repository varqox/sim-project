#pragma once

#include "job_handler.hh"

namespace job_server::job_handlers {

class DeleteInternalFile final : public JobHandler {
public:
    explicit DeleteInternalFile(uint64_t job_id) : JobHandler(job_id) {}

    void run(sim::mysql::Connection& mysql) final;
};

} // namespace job_server::job_handlers
