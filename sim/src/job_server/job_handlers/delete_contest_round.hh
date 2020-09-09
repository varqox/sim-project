#pragma once

#include "job_handler.hh"

namespace job_handlers {

class DeleteContestRound final : public JobHandler {
	uint64_t contest_round_id_;

public:
	DeleteContestRound(uint64_t job_id, uint64_t contest_round_id)
	: JobHandler(job_id)
	, contest_round_id_(contest_round_id) {}

	void run() final;
};

} // namespace job_handlers
