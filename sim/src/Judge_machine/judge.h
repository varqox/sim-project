#pragma once

#include <string>

struct JudgeResult {
	enum Status : uint8_t { OK, ERROR, COMPILE_ERROR, JUDGE_ERROR } status;
	int64_t score;
	std::string initial_report;
	std::string final_report;

	JudgeResult(Status st, long long points, const std::string& initial = "",
		const std::string& final = "") : status(st), score(points),
			initial_report(initial), final_report(final) {}
};

JudgeResult judge(std::string submission_id, std::string problem_id);
