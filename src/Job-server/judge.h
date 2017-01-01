#pragma once

#include <simlib/string.h>

void judgeSubmission(const std::string& job_id,
	const std::string& submission_id, StringView info);

void judgeModelSolution(const std::string& job_id);
