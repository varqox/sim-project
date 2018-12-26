#include "sip_error.h"
#include "sipfile.h"

// Macros because of string concentration in compile-time (in C++11 it is hard
// to achieve in the other way and this macros are pretty more readable than
// some template meta-programming code that concentrates string literals. Also,
// the macros are used only locally, so after all, they are not so evil...)
#define CHECK_IF_ARR(var, name) if (!var.isArray() && var.isSet()) \
	throw SipError("Sipfile: variable `" name "` has to be" \
		" specified as an array")
#define CHECK_IF_NOT_ARR(var, name) if (var.isArray()) \
	throw SipError("Sipfile: variable `" name "` cannot be" \
		" specified as an array")

void Sipfile::loadDefaultTimeLimit() {
	auto&& dtl = config["default_time_limit"];
	CHECK_IF_NOT_ARR(dtl, "default_time_limit");

	if (dtl.asString().empty())
		throw SipError("Sipfile: missing default_time_limit");

	auto x = dtl.asString();
	if (!isReal(x))
		throw SipError("Sipfile: invalid default time limit");

	double tl = stod(x);
	if (tl <= 0)
		throw SipError("Sipfile: default time limit has to be grater than 0");

	default_time_limit = round(tl * 1000000LL);
	if (default_time_limit == 0) {
		throw SipError("Sipfile: default time limit is to small - after"
			" rounding it is equal to 0 microseconds, but it has to be at least"
			" 1 microsecond");
	}
}
