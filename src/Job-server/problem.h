#pragma once

#include <sim/constants.h>
#include <simlib/string.h>

void addProblem(uint64_t job_id, StringView job_owner, StringView info);

void reuploadProblem(uint64_t job_id, StringView job_owner, StringView info,
	StringView aux_id);
