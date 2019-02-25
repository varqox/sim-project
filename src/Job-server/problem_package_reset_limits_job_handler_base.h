#pragma once

#include "judge_job_handler_base.h"

class ProblemPackageResetLimitsJobHandlerBase : public JudgeJobHandlerBase {
protected:
	std::string new_simfile;

	ProblemPackageResetLimitsJobHandlerBase() = default;

	// Sets new_simfile to the new simfile's dump
	void reset_package_time_limits(FilePath package_path);

public:
	~ProblemPackageResetLimitsJobHandlerBase() = default;
};
