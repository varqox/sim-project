#pragma once

#include "constants.h"

#include <simlib/mysql.h>

namespace submission {

void update_final(MySQL::Connection& mysql, StringView user_id,
	StringView round_id, bool lock_table = true)
{
	auto impl = [&] {
		/* Select ids of all submissions that will be affected */

		// We cannot do it using UNION, because (stupid) MysQL does not allow it
		// under the lock
		auto stmt = mysql.prepare("SELECT id FROM submissions"
			" WHERE owner=? AND round_id=? AND final_candidate=1"
			" ORDER BY id DESC LIMIT 1");
		stmt.bindAndExecute(user_id, round_id);

		uint64_t new_final_id;
		stmt.res_bind_all(new_final_id);
		if (not stmt.next())
			return; // Nothing to do

		auto ustmt = mysql.prepare("UPDATE submissions SET type=? WHERE id=?");

		uint64_t curr_id = new_final_id;
		uint type = uint(SubmissionType::FINAL);
		ustmt.bindAndExecute(type, curr_id); // Set final

		// Unset older finals
		type = uint(SubmissionType::NORMAL);
		stmt = mysql.prepare("SELECT id FROM submissions WHERE owner=?"
			" AND round_id=? AND type=" STYPE_FINAL_STR);
		stmt.bindAndExecute(user_id, round_id);
		stmt.res_bind_all(curr_id);

		while (stmt.next())
			if (curr_id != new_final_id)
				ustmt.execute();
	};

	if (lock_table) {
		mysql.update("LOCK TABLES submissions WRITE");
		auto lock_guard = make_call_in_destructor([&]{
			mysql.update("UNLOCK TABLES");
		});
		impl();

	} else
		impl();
}

} // namespace submission
