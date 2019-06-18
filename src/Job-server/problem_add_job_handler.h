#pragma once

#include "problem_add_or_reupload_job_handler_base.h"

class ProblemAddJobHandler final : public ProblemAddOrReuploadJobHandlerBase {
public:
	ProblemAddJobHandler(uint64_t job_id, StringView job_creator,
	                     const jobs::AddProblemInfo& info, uint64_t job_file_id,
	                     Optional<uint64_t> tmp_file_id)
	   : JobHandler(job_id),
	     ProblemAddOrReuploadJobHandlerBase(JobType::ADD_PROBLEM, job_creator,
	                                        info, job_file_id, tmp_file_id,
	                                        std::nullopt) {}

	void run() override;
};
