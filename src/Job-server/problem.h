#pragma once

#include <sim/constants.h>
#include <sim/jobs.h>
#include <simlib/string.h>

void add_problem(uint64_t job_id, StringView job_owner, StringView info);

void reupload_problem(uint64_t job_id, StringView job_owner, StringView info,
	StringView problem_id);

void delete_problem(uint64_t job_id, StringView problem_id);

void merge_problem_into_another(uint64_t job_id, StringView problem_id,
	jobs::MergeProblemsInfo info);
