#pragma once

#include <simlib/string.h>
#include <sim/constants.h>

void judgeSubmission(uint64_t job_id, StringView submission_id,
	StringView job_creation_time);

void judgeModelSolution(uint64_t job_id, JobType original_job_type);
