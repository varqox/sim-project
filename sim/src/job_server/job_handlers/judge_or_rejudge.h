#pragma once

#include "judge_base.h"

namespace job_handlers {

class JudgeOrRejudge final : public JudgeBase {
private:
	const uint64_t submission_id;
	const StringView job_creation_time;

public:
	JudgeOrRejudge(uint64_t job_id, uint64_t submission_id,
	               StringView job_creation_time)
	   : JobHandler(job_id), submission_id(submission_id),
	     job_creation_time(job_creation_time) {}

	void run() override;
};

} // namespace job_handlers
