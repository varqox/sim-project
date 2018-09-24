#pragma once

#include <simlib/string.h>
#include <sim/constants.h>

void judge_submission(uint64_t job_id, StringView submission_id,
	StringView job_creation_time);

void problem_add_or_reupload_jugde_model_solution(uint64_t job_id,
	JobType original_job_type);

void reset_problem_time_limits_using_model_solution(uint64_t job_id,
	StringView problem_id);
