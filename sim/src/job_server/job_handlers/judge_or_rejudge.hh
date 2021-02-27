#pragma once

#include "src/job_server/job_handlers/judge_base.hh"

namespace job_server::job_handlers {

class JudgeOrRejudge final : public JudgeBase {
private:
    const uint64_t submission_id_;
    const StringView job_creation_time_;

public:
    JudgeOrRejudge(uint64_t job_id, uint64_t submission_id, StringView job_creation_time)
    : JobHandler(job_id)
    , submission_id_(submission_id)
    , job_creation_time_(job_creation_time) {}

    void run() override;
};

} // namespace job_server::job_handlers
