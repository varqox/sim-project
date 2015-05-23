#pragma once

#include <string>

struct JudgeResult {
	enum Status { OK, ERROR, CERROR } status;
	long long score;
	std::string content;
};

JudgeResult judge(std::string submission_id, std::string problem_id);
