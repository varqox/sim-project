#pragma once

#include <sim/constants.h>
#include <simlib/string.h>

void addProblem(StringView job_id, StringView job_owner, StringView info);

void reuploadProblem(StringView job_id, StringView job_owner, StringView info,
	StringView aux_id);
