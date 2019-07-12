#pragma once

#include "job_handler.h"

namespace job_handlers {

class DeleteContestRound final : public JobHandler {
	uint64_t contest_round_id;

public:
	DeleteContestRound(uint64_t job_id, uint64_t contest_round_id)
	   : JobHandler(job_id), contest_round_id(contest_round_id) {}

	void run() override final;
};

} // namespace job_handlers
