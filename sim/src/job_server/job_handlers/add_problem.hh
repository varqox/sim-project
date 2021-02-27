#pragma once

#include "src/job_server/job_handlers/add_or_reupload_problem_base.hh"

namespace job_server::job_handlers {

class AddProblem final : public AddOrReuploadProblemBase {
public:
    AddProblem(
        uint64_t job_id, StringView job_creator, const sim::jobs::AddProblemInfo& info,
        uint64_t job_file_id, std::optional<uint64_t> tmp_file_id)
    : JobHandler(job_id)
    , AddOrReuploadProblemBase(
          sim::JobType::ADD_PROBLEM, job_creator, info, job_file_id, tmp_file_id,
          std::nullopt) {}

    void run() override;
};

} // namespace job_server::job_handlers
