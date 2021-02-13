#pragma once

#include "add_or_reupload_problem_base.hh"

namespace job_handlers {

class ReuploadProblem final : public AddOrReuploadProblemBase {
public:
    ReuploadProblem(
        uint64_t job_id, StringView job_creator, const jobs::AddProblemInfo& info,
        uint64_t job_file_id, std::optional<uint64_t> tmp_file_id, uint64_t problem_id)
    : JobHandler(job_id)
    , AddOrReuploadProblemBase(
          JobType::REUPLOAD_PROBLEM, job_creator, info, job_file_id, tmp_file_id, problem_id) {
    }

    void run() override;
};

} // namespace job_handlers
