#pragma once

#include <simlib/string.h>
#include <sim/constants.h>

void judgeSubmission(const std::string& job_id,
	const std::string& submission_id, const std::string& job_creation_time);

void judgeModelSolution(const std::string& job_id, bool reupload_problem);
