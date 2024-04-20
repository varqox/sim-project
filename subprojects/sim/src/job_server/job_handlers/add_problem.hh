#pragma once

#include "add_or_reupload_problem_base.hh"

#include <sim/jobs/old_job.hh>

namespace job_server::job_handlers {

class AddProblem final : public AddOrReuploadProblemBase {
public:
    AddProblem(
        sim::mysql::Connection& mysql,
        uint64_t job_id,
        StringView job_creator,
        const sim::jobs::AddProblemInfo& info,
        uint64_t job_file_id,
        std::optional<uint64_t> tmp_file_id
    )
    : JobHandler(job_id)
    , AddOrReuploadProblemBase(
          mysql,
          sim::jobs::OldJob::Type::ADD_PROBLEM,
          job_creator,
          info,
          job_file_id,
          tmp_file_id,
          std::nullopt
      ) {}

    void run(sim::mysql::Connection& mysql) override;
};

} // namespace job_server::job_handlers
