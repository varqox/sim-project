#pragma once

#include <algorithm>
#include <string>

// In <cppconn/sqlstring.h> in function sql::SQLString::caseCompare() there is
// a problem with type deduction of ::tolower, the code below fixes it if is
// included before <cppconn/sqlstring.h>
namespace std {

inline string::iterator transform(string::iterator first1,
	string::iterator last1, string::iterator first, int(*f)(int))
{
	return std::transform(first1, last1, first, [&](int x){ return f(x); });
}

} // std
