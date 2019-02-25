#pragma once

#include "job_handler.h"

class DeleteContestRoundJobHandler final : public JobHandler {
	uint64_t contest_round_id;

public:
	DeleteContestRoundJobHandler(uint64_t job_id, uint64_t contest_round_id) : JobHandler(job_id), contest_round_id(contest_round_id) {}

	void run() override final;
};
