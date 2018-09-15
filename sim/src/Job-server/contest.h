#pragma once

#include <simlib/string.h>

void contest_problem_reselect_final_submissions(uint64_t job_id,
	StringView contest_problem_id);

void delete_contest(uint64_t job_id, StringView contest_id);

void delete_contest_round(uint64_t job_id, StringView contest_round_id);

void delete_contest_problem(uint64_t job_id, StringView contest_problem_id);
