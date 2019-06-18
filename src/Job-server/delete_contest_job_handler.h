#pragma once

#include "job_handler.h"

class DeleteContestJobHandler final : public JobHandler {
	uint64_t contest_id;

public:
	DeleteContestJobHandler(uint64_t job_id, uint64_t contest_id)
	   : JobHandler(job_id), contest_id(contest_id) {}

	void run() override final;
};
