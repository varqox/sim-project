#pragma once

#include "job_handler.h"

class ContestProblemReselectFinalSubmissionsJobHandler final : public JobHandler {
	uint64_t contest_problem_id;

public:
	ContestProblemReselectFinalSubmissionsJobHandler(uint64_t job_id, uint64_t contest_problem_id) : JobHandler(job_id), contest_problem_id(contest_problem_id) {}

	void run() override final;
};
