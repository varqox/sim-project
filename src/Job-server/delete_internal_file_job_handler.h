#pragma once

#include "job_handler.h"

class DeleteInternalFileJobHandler final : public JobHandler {
	uint64_t internal_file_id;

public:
	DeleteInternalFileJobHandler(uint64_t job_id, uint64_t internal_file_id)
		: JobHandler(job_id), internal_file_id(internal_file_id) {}

	void run() override final;
};
