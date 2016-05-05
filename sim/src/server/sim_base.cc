#include "sim_base.h"

using std::string;

string SimBase::submissionStatusDescription(const string& status) {
	if (status == "ok")
		return "Initial tests: OK";

	if (status == "error")
		return "Initial tests: Error";

	if (status == "c_error")
		return "Compilation failed";

	if (status == "judge_error")
		return "Judge error";

	if (status == "waiting")
		return "Pending";

	return "Unknown";
}
