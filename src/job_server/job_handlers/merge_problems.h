#pragma once

#include "job_handler.h"

#include <sim/jobs.h>

namespace job_handlers {

class MergeProblems final : public JobHandler {
	const uint64_t problem_id;
	const jobs::MergeProblemsInfo info;

public:
	MergeProblems(uint64_t job_id, uint64_t problem_id,
	              const jobs::MergeProblemsInfo& info)
	   : JobHandler(job_id), problem_id(problem_id), info(info) {}

	void run() override final;
};

} // namespace job_handlers
