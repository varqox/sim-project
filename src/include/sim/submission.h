#pragma once

#include <simlib/mysql.h>

namespace submission {

void update_final(MySQL::Connection& mysql, Optional<uint64_t> submission_owner,
	uint64_t problem_id, Optional<uint64_t> contest_problem_id,
	bool make_transaction = true);

} // namespace submission
