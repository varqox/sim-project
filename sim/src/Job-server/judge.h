#pragma once

#include <simlib/string.h>
#include <sim/constants.h>

void judgeSubmission(StringView job_id, StringView submission_id,
	StringView job_creation_time);

void judgeModelSolution(StringView job_id,
	JobQueueType original_job_type);
