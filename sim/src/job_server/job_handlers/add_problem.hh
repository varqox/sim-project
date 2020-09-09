#pragma once

#include "add_or_reupload_problem_base.hh"

namespace job_handlers {

class AddProblem final : public AddOrReuploadProblemBase {
public:
	AddProblem(uint64_t job_id, StringView job_creator,
	           const jobs::AddProblemInfo& info, uint64_t job_file_id,
	           std::optional<uint64_t> tmp_file_id)
	   : JobHandler(job_id),
	     AddOrReuploadProblemBase(JobType::ADD_PROBLEM, job_creator, info,
	                              job_file_id, tmp_file_id, std::nullopt) {}

	void run() override;
};

} // namespace job_handlers
