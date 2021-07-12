#pragma once

#include "sim/jobs/utils.hh"
#include "src/job_server/job_handlers/job_handler.hh"

namespace job_server::job_handlers {

class MergeUsers final : public JobHandler {
    const uint64_t donor_user_id_;
    const sim::jobs::MergeUsersInfo info_;

public:
    MergeUsers(uint64_t job_id, uint64_t donor_user_id, const sim::jobs::MergeUsersInfo& info)
    : JobHandler(job_id)
    , donor_user_id_(donor_user_id)
    , info_(info) {}

    void run() final;

private:
    void run_impl();
};

} // namespace job_server::job_handlers
