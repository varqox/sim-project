#pragma once

#include "problem_add_or_reupload_job_handler_base.h"

class ProblemReuploadJobHandler final : public ProblemAddOrReuploadJobHandlerBase {
public:
	ProblemReuploadJobHandler(uint64_t job_id, StringView job_creator,
			const jobs::AddProblemInfo& info, uint64_t job_file_id,
			Optional<uint64_t> tmp_file_id, uint64_t problem_id)
		: JobHandler(job_id),
			ProblemAddOrReuploadJobHandlerBase(JobType::REUPLOAD_PROBLEM,
				job_creator, info, job_file_id, tmp_file_id, problem_id) {}

	void run() override;
};
