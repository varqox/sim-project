#pragma once

#include "job_handler.h"

class DeleteUserJobHandler final : public JobHandler {
	uint64_t user_id;

public:
	DeleteUserJobHandler(uint64_t job_id, uint64_t user_id)
	   : JobHandler(job_id), user_id(user_id) {}

	void run() override final;
};
