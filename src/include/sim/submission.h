#pragma once

#include "constants.h"

#include <simlib/mysql.h>

namespace submission {

inline void update_final(MySQL::Connection& mysql, StringView submission_owner,
	StringView contest_problem_id, bool lock_tables = true)
{
	if (submission_owner.empty()) // System submission
		return; // Nothing to do

	if (contest_problem_id.empty())
		return; // TODO: problemset submissions

	auto impl = [&] {
		STACK_UNWINDING_MARK;
		using SFSM = SubmissionFinalSelectingMethod;

		// Get the final selecting method
		auto stmt = mysql.prepare("SELECT final_selecting_method FROM contest_problems WHERE id=?");
		stmt.bindAndExecute(contest_problem_id);
		std::underlying_type_t<SFSM> final_method_u;
		stmt.res_bind_all(final_method_u);
		if (not stmt.next())
			return; // Such contest problem does not exist (probably had just
			        // been deleted)

		/* Select ids of all submissions that will be affected
		   We cannot do it using UNION, because (stupid) MySQL does not allow it
		   under the lock, so the new final is selected and then all other
		   finals */
		switch (SFSM(final_method_u)) {
		case SFSM::LAST_COMPILING:
			stmt = mysql.prepare("SELECT id FROM submissions USE INDEX(final1)"
				" WHERE owner=? AND contest_problem_id=? AND final_candidate=1"
				" ORDER BY id DESC LIMIT 1");
			stmt.bindAndExecute(submission_owner, contest_problem_id);
			break;

		case SFSM::WITH_HIGHEST_SCORE:
			// Subqueries are used because MySQL does not support descending
			// indexes and we want an efficient query, below there is used a
			// heuristic: of submissions that have the same score the one with
			// smaller status value is better
			// Queries are split because under the write lock the subqueries
			// does not work
			int64_t score;
			stmt = mysql.prepare("SELECT score"
				" FROM submissions USE INDEX(final2)"
				" WHERE final_candidate=1 AND owner=? AND contest_problem_id=?"
				" ORDER BY score DESC LIMIT 1");
			stmt.bindAndExecute(submission_owner, contest_problem_id);
			stmt.res_bind_all(score);
			if (not stmt.next())
				return; // Nothing to do (no submission that may be final)

			std::underlying_type_t<SubmissionStatus> status;
			stmt = mysql.prepare("SELECT status"
				" FROM submissions USE INDEX(final2)"
				" WHERE final_candidate=1 AND owner=? AND contest_problem_id=?"
					" AND score=?"
				" ORDER BY status LIMIT 1");
			stmt.bindAndExecute(submission_owner, contest_problem_id, score);
			stmt.res_bind_all(status);
			throw_assert(stmt.next()); // Above query succeeded, so this has to

			stmt = mysql.prepare("SELECT id FROM submissions USE INDEX(final2)"
				" WHERE final_candidate=1 AND owner=? AND contest_problem_id=?"
					" AND score=? AND status=?"
				" ORDER BY id DESC LIMIT 1");
			stmt.bindAndExecute(submission_owner, contest_problem_id, score,
				status);
			break;
		}


		uint64_t new_final_id;
		stmt.res_bind_all(new_final_id);
		if (not stmt.next())
			return; // Nothing to do (no submission that may be final)

		auto ustmt = mysql.prepare("UPDATE submissions SET type=? WHERE id=?");

		uint64_t curr_id = new_final_id;
		auto type = std::underlying_type_t<SubmissionType>(SubmissionType::FINAL);
		ustmt.bindAndExecute(type, curr_id); // Set final

		// Unset older finals
		type = uint(SubmissionType::NORMAL);
		stmt = mysql.prepare("SELECT id FROM submissions WHERE owner=?"
			" AND contest_problem_id=? AND type=" STYPE_FINAL_STR);
		stmt.bindAndExecute(submission_owner, contest_problem_id);
		stmt.res_bind_all(curr_id);

		while (stmt.next())
			if (curr_id != new_final_id)
				ustmt.execute();
	};

	if (lock_tables) {
		mysql.update("LOCK TABLES submissions WRITE, contest_problems READ");
		auto lock_guard = make_call_in_destructor([&]{
			mysql.update("UNLOCK TABLES");
		});
		impl();

	} else
		impl();
}

} // namespace submission
