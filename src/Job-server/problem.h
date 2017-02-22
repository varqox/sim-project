#pragma once

#include <sim/constants.h>
#include <simlib/string.h>

void addProblem(const std::string& job_id, const std::string& job_owner,
	StringView info);

void reuploadProblem(const std::string& job_id, const std::string& job_owner,
	StringView info, const std::string& aux_id);
