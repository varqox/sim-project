#pragma once

#include <simlib/mysql.h>

namespace submission {

void update_final(MySQL::Connection& mysql, StringView submission_owner,
	StringView problem_id, StringView contest_problem_id,
	bool lock_tables = true);

} // namespace submission
