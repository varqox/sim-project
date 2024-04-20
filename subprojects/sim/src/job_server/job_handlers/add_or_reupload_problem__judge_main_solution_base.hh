#pragma once

#include "add_or_reupload_problem_base.hh"
#include "reset_time_limits_in_problem_package_base.hh"

namespace job_server::job_handlers {

class AddOrReuploadProblemJudgeModelSolutionBase
: public ResetTimeLimitsInProblemPackageBase
, public AddOrReuploadProblemBase {
protected:
    AddOrReuploadProblemJudgeModelSolutionBase(
        sim::mysql::Connection& mysql,
        uint64_t job_id,
        sim::jobs::OldJob::Type job_type,
        StringView job_creator,
        const sim::jobs::AddProblemInfo& info,
        uint64_t job_file_id,
        std::optional<uint64_t> tmp_file_id,
        std::optional<uint64_t> problem_id
    )
    : JobHandler(job_id)
    , AddOrReuploadProblemBase(
          mysql, job_type, job_creator, info, job_file_id, tmp_file_id, problem_id
      ) {}

public:
    void run(sim::mysql::Connection& mysql) final;
};

} // namespace job_server::job_handlers
