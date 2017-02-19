#pragma once

#include <simlib/string.h>

void addOrReuploadProblem(const std::string& job_id,
	const std::string& job_owner, StringView info, const std::string& aux_id,
	bool reupload_problem);
