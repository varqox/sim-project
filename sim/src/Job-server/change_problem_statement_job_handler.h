#pragma once

#include "job_handler.h"

#include <sim/jobs.h>

class ChangeProblemStatementJobHandler final : public JobHandler {
	uint64_t problem_id;
	uint64_t job_file_id;
	jobs::ChangeProblemStatementInfo info;

public:
	ChangeProblemStatementJobHandler(
	   uint64_t job_id, uint64_t problem_id, uint64_t job_file_id,
	   const jobs::ChangeProblemStatementInfo& info)
	   : JobHandler(job_id), problem_id(problem_id), job_file_id(job_file_id),
	     info(info) {}

	void run() override final;
};
