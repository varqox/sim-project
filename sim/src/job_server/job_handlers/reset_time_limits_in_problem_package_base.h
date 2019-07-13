#pragma once

#include "judge_base.h"

namespace job_handlers {

class ResetTimeLimitsInProblemPackageBase : public JudgeBase {
protected:
	std::string new_simfile;

	ResetTimeLimitsInProblemPackageBase() = default;

	// Sets new_simfile to the new simfile's dump
	void reset_package_time_limits(FilePath package_path);

public:
	~ResetTimeLimitsInProblemPackageBase() = default;
};

} // namespace job_handlers
