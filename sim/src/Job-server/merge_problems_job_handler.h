#pragma once

#include "job_handler.h"

#include <sim/jobs.h>

class MergeProblemsJobHandler final : public JobHandler {
	const uint64_t problem_id;
	const jobs::MergeProblemsInfo info;

public:
	MergeProblemsJobHandler(uint64_t job_id, uint64_t problem_id,
	                        const jobs::MergeProblemsInfo& info)
	   : JobHandler(job_id), problem_id(problem_id), info(info) {}

	void run() override final;
};
