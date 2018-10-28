#pragma once

#include <simlib/mysql.h>

namespace submission {

// TODO: owner and contest_problem_id should be an Optional
void update_final(MySQL::Connection& mysql, StringView submission_owner,
	StringView problem_id, StringView contest_problem_id,
	bool lock_tables = true);

void delete_submission(MySQL::Connection& mysql, StringView submission_id);

} // namespace submission
