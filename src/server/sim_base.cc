#include "sim_base.h"

using std::string;

string SimBase::submissionStatusAsTd(SubmissionStatus status,
	bool show_final)
{
	// Fatal
	if (status >= SubmissionStatus::WAITING) {
		if (status == SubmissionStatus::WAITING)
			return "<td class=\"status\">Pending</td>";

		if (status == SubmissionStatus::COMPILATION_ERROR)
			return "<td class=\"status purple\">Compilation failed</td>";

		if (status == SubmissionStatus::CHECKER_COMPILATION_ERROR)
			return "<td class=\"status blue\">Checker compilation failed</td>";

		if (status == SubmissionStatus::JUDGE_ERROR)
			return "<td class=\"status blue\">Judge error</td>";

		return "<td class=\"status\">Unknown</td>";
	}

	/* Non-fatal */

	// Final
	if (show_final) {
		if ((status & SubmissionStatus::FINAL_MASK) == SubmissionStatus::OK)
			return "<td class=\"status green\">OK</td>";

		if ((status & SubmissionStatus::FINAL_MASK) == SubmissionStatus::WA)
			return "<td class=\"status red\">Wrong answer</td>";

		if ((status & SubmissionStatus::FINAL_MASK) == SubmissionStatus::TLE)
			return "<td class=\"status yellow\">Time limit exceeded</td>";

		if ((status & SubmissionStatus::FINAL_MASK) == SubmissionStatus::MLE)
			return "<td class=\"status yellow\">Memory limit exceeded</td>";

		if ((status & SubmissionStatus::FINAL_MASK) == SubmissionStatus::RTE)
			return "<td class=\"status intense-red\">Runtime error</td>";

	// Initial
	} else {
		if ((status & SubmissionStatus::INITIAL_MASK) ==
			SubmissionStatus::INITIAL_OK)
		{
			return "<td class=\"status green\">Initial tests: OK</td>";
		}

		if ((status & SubmissionStatus::INITIAL_MASK) ==
			SubmissionStatus::INITIAL_WA)
		{
			return "<td class=\"status red\">Initial tests: Wrong answer</td>";
		}

		if ((status & SubmissionStatus::INITIAL_MASK) ==
			SubmissionStatus::INITIAL_TLE)
		{
			return "<td class=\"status yellow\">"
				"Initial tests: Time limit exceeded</td>";
		}

		if ((status & SubmissionStatus::INITIAL_MASK) ==
			SubmissionStatus::INITIAL_MLE)
		{
			return "<td class=\"status yellow\">"
				"Initial tests: Memory limit exceeded</td>";
		}

		if ((status & SubmissionStatus::INITIAL_MASK) ==
			SubmissionStatus::INITIAL_RTE)
		{
			return "<td class=\"status intense-red\">"
				"Initial tests: Runtime error</td>";
		}
	}

	return "<td class=\"status\">Unknown</td>";
}

string SimBase::submissionStatusCSSClass(SubmissionStatus status,
	bool show_final)
{
	// Fatal
	if (status >= SubmissionStatus::WAITING) {
		if (status == SubmissionStatus::WAITING)
			return "status";

		if (status == SubmissionStatus::COMPILATION_ERROR)
			return "status purple";

		if (status == SubmissionStatus::CHECKER_COMPILATION_ERROR)
			return "status blue";

		if (status == SubmissionStatus::JUDGE_ERROR)
			return "status blue";

		return "status";
	}

	/* Non-fatal */

	// Final
	if (show_final) {
		if ((status & SubmissionStatus::FINAL_MASK) == SubmissionStatus::OK)
			return "status green";

		if ((status & SubmissionStatus::FINAL_MASK) == SubmissionStatus::WA)
			return "status red";

		if ((status & SubmissionStatus::FINAL_MASK) == SubmissionStatus::TLE)
			return "status yellow";

		if ((status & SubmissionStatus::FINAL_MASK) == SubmissionStatus::MLE)
			return "status yellow";

		if ((status & SubmissionStatus::FINAL_MASK) == SubmissionStatus::RTE)
			return "status intense-red";

	// Initial
	} else {
		if ((status & SubmissionStatus::INITIAL_MASK) ==
			SubmissionStatus::INITIAL_OK)
		{
			return "status green";
		}

		if ((status & SubmissionStatus::INITIAL_MASK) ==
			SubmissionStatus::INITIAL_WA)
		{
			return "status red";
		}

		if ((status & SubmissionStatus::INITIAL_MASK) ==
			SubmissionStatus::INITIAL_TLE)
		{
			return "status yellow";
		}

		if ((status & SubmissionStatus::INITIAL_MASK) ==
			SubmissionStatus::INITIAL_MLE)
		{
			return "status yellow";
		}

		if ((status & SubmissionStatus::INITIAL_MASK) ==
			SubmissionStatus::INITIAL_RTE)
		{
			return "status intense-red";
		}
	}

	return "status";
}
