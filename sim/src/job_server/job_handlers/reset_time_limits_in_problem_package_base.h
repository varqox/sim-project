#pragma once

#include "judge_base.h"

namespace job_handlers {

class ResetTimeLimitsInProblemPackageBase : public JudgeBase {
protected:
	std::string new_simfile_;

	// ResetTimeLimitsInProblemPackageBase() = default; // Bug in GCC:
	// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=91159

	// Sets new_simfile to the new simfile's dump
	void reset_package_time_limits(FilePath package_path);
};

} // namespace job_handlers
