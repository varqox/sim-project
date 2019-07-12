#pragma once

#include "job_handler.h"

namespace job_handlers {

class DeleteInternalFile final : public JobHandler {
	uint64_t internal_file_id;

public:
	DeleteInternalFile(uint64_t job_id, uint64_t internal_file_id)
	   : JobHandler(job_id), internal_file_id(internal_file_id) {}

	void run() override final;
};

} // namespace job_handlers
