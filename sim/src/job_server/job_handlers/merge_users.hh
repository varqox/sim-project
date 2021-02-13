#pragma once

#include "job_handler.hh"

#include <sim/jobs.hh>

namespace job_handlers {

class MergeUsers final : public JobHandler {
    const uint64_t donor_user_id_;
    const jobs::MergeUsersInfo info_;

public:
    MergeUsers(uint64_t job_id, uint64_t donor_user_id, const jobs::MergeUsersInfo& info)
    : JobHandler(job_id)
    , donor_user_id_(donor_user_id)
    , info_(info) {}

    void run() final;

private:
    void run_impl();
};

} // namespace job_handlers
