#pragma once

#include <simlib/string.h>

void addProblem(const std::string& job_id, const std::string& job_owner,
	StringView info);
