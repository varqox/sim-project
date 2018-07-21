#include <sim/constants.h>
#include <sim/submission.h>

static void update_problem_final(MySQL::Connection& mysql,
	StringView submission_owner, StringView problem_id)
{
	STACK_UNWINDING_MARK;

	// Such index: (final_candidate, owner, problem_id, score DESC, full_status, id DESC)
	// is what we need, but MySQL does not support it, so the below workaround is used to select the final submission efficiently
	auto stmt = mysql.prepare("SELECT score FROM submissions USE INDEX(final3)"
		" WHERE final_candidate=1 AND owner=? AND problem_id=?"
		" ORDER BY score DESC LIMIT 1");
	stmt.bindAndExecute(submission_owner, problem_id);

	int64_t final_score;
	stmt.res_bind_all(final_score);
	if (not stmt.next()) {
		// Unset final submissions if there are any because there are no
		// candidates now
		stmt = mysql.prepare("UPDATE submissions SET problem_final=0"
			" WHERE owner=? AND problem_id=? AND problem_final=1");
		stmt.bindAndExecute(submission_owner, problem_id);
		return; // Nothing more to be done
	}

	std::underlying_type_t<SubmissionStatus> full_status;
	stmt = mysql.prepare("SELECT full_status"
		" FROM submissions USE INDEX(final3)"
		" WHERE final_candidate=1 AND owner=? AND problem_id=? AND score=?"
		" ORDER BY full_status LIMIT 1");
	stmt.bindAndExecute(submission_owner, problem_id, final_score);
	stmt.res_bind_all(full_status);
	throw_assert(stmt.next()); // Previous query succeeded, so this has to

	// Choose the new final submission
	stmt = mysql.prepare("SELECT id FROM submissions USE INDEX(final3)"
		" WHERE final_candidate=1 AND owner=? AND problem_id=? AND score=?"
			" AND full_status=?"
		" ORDER BY id DESC LIMIT 1");
	stmt.bindAndExecute(submission_owner, problem_id, final_score, full_status);

	uint64_t new_final_id;
	stmt.res_bind_all(new_final_id);
	throw_assert(stmt.next()); // Previous query succeeded, so this has to

	// Update finals
	stmt = mysql.prepare("UPDATE submissions SET problem_final=IF(id=?, 1, 0)"
		" WHERE owner=? AND problem_id=? AND (problem_final=1 OR id=?)");
	stmt.bindAndExecute(new_final_id, submission_owner, problem_id, new_final_id);
}

static void update_contest_final(MySQL::Connection& mysql,
	StringView submission_owner, StringView contest_problem_id)
{
	STACK_UNWINDING_MARK;
	using SFSM = SubmissionFinalSelectingMethod;

	// Get the final selecting method and whether the score is revealed
	auto stmt = mysql.prepare("SELECT final_selecting_method, reveal_score"
		" FROM contest_problems WHERE id=?");
	stmt.bindAndExecute(contest_problem_id);

	std::underlying_type_t<SFSM> final_method_u;
	bool reveal_score;
	stmt.res_bind_all(final_method_u, reveal_score);
	if (not stmt.next())
		return; // Such contest problem does not exist (probably had just
		        // been deleted)

	auto unset_all_finals = [&] {
		// Unset final submissions if there are any because there are no
		// candidates now
		stmt = mysql.prepare("UPDATE submissions"
			" SET contest_final=0, contest_initial_final=0"
			" WHERE owner=? AND contest_problem_id=? AND (contest_final=1 OR contest_initial_final=1)");
		stmt.bindAndExecute(submission_owner, contest_problem_id);
	};

	uint64_t new_final_id, new_initial_final_id;
	switch (SFSM(final_method_u)) {
	case SFSM::LAST_COMPILING: {
		// Choose the new final submission
		stmt = mysql.prepare("SELECT id FROM submissions USE INDEX(final1)"
			" WHERE owner=? AND contest_problem_id=? AND final_candidate=1"
			" ORDER BY id DESC LIMIT 1");
		stmt.bindAndExecute(submission_owner, contest_problem_id);
		stmt.res_bind_all(new_final_id);
		if (not stmt.next()) // Nothing to do (no submission that may be final)
			return unset_all_finals();

		new_initial_final_id = new_final_id;
		break;
	}
	case SFSM::WITH_HIGHEST_SCORE: {
		// Such index: (final_candidate, owner, contest_problem_id, score DESC,
		// full_status, id DESC) is what we need, but MySQL does not support it,
		// so the below workaround is used to select the final submission efficiently
		int64_t final_score;
		stmt = mysql.prepare("SELECT score FROM submissions USE INDEX(final2)"
			" WHERE final_candidate=1 AND owner=? AND contest_problem_id=?"
			" ORDER BY score DESC LIMIT 1");
		stmt.bindAndExecute(submission_owner, contest_problem_id);
		stmt.res_bind_all(final_score);
		if (not stmt.next()) // Nothing to do (no submission that may be final)
			return unset_all_finals();

		std::underlying_type_t<SubmissionStatus> full_status;
		stmt = mysql.prepare("SELECT full_status"
			" FROM submissions USE INDEX(final2)"
			" WHERE final_candidate=1 AND owner=? AND contest_problem_id=?"
				" AND score=?"
			" ORDER BY full_status LIMIT 1");
		stmt.bindAndExecute(submission_owner, contest_problem_id, final_score);
		stmt.res_bind_all(full_status);
		throw_assert(stmt.next()); // Previous query succeeded, so this has to

		// Choose the new final submission
		stmt = mysql.prepare("SELECT id FROM submissions USE INDEX(final2)"
			" WHERE final_candidate=1 AND owner=? AND contest_problem_id=?"
				" AND score=? AND full_status=?"
			" ORDER BY id DESC LIMIT 1");
		stmt.bindAndExecute(submission_owner, contest_problem_id, final_score,
			full_status);
		stmt.res_bind_all(new_final_id);
		throw_assert(stmt.next()); // Previous query succeeded, so this has to

		// Choose the new initial final submission
		if (reveal_score) {
			// Such index: (final_candidate, owner, contest_problem_id,
			// score DESC, initial_status, id DESC) is what we need, but MySQL
			// does not support it, so the below workaround is used to select
			// the initial final submission efficiently
			std::underlying_type_t<SubmissionStatus> initial_status;
			stmt = mysql.prepare("SELECT initial_status"
				" FROM submissions USE INDEX(initial_final3)"
				" WHERE final_candidate=1 AND owner=? AND contest_problem_id=?"
					" AND score=?"
				" ORDER BY initial_status LIMIT 1");
			stmt.bindAndExecute(submission_owner, contest_problem_id, final_score);
			stmt.res_bind_all(initial_status);
			throw_assert(stmt.next()); // Previous query succeeded, so this has to

			stmt = mysql.prepare("SELECT id"
				" FROM submissions USE INDEX(initial_final3)"
				" WHERE final_candidate=1 AND owner=? AND contest_problem_id=?"
					" AND score=? AND initial_status=?"
				" ORDER BY id DESC LIMIT 1");
			stmt.bindAndExecute(submission_owner, contest_problem_id, final_score,
				initial_status);
			stmt.res_bind_all(new_initial_final_id);
			throw_assert(stmt.next()); // Previous query succeeded, so this has to

		} else {
			// Such index: (final_candidate, owner, contest_problem_id,
			// initial_status, id DESC) is what we need, but MySQL does not
			// support it, so the below workaround is used to select the initial
			// final submission efficiently
			std::underlying_type_t<SubmissionStatus> initial_status;
			stmt = mysql.prepare("SELECT initial_status"
				" FROM submissions USE INDEX(initial_final2)"
				" WHERE final_candidate=1 AND owner=? AND contest_problem_id=?"
				" ORDER BY initial_status LIMIT 1");
			stmt.bindAndExecute(submission_owner, contest_problem_id);
			stmt.res_bind_all(initial_status);
			throw_assert(stmt.next()); // Previous query succeeded, so this has to

			stmt = mysql.prepare("SELECT id"
				" FROM submissions USE INDEX(initial_final2)"
				" WHERE final_candidate=1 AND owner=? AND contest_problem_id=?"
					" AND initial_status=?"
				" ORDER BY id DESC LIMIT 1");
			stmt.bindAndExecute(submission_owner, contest_problem_id,
				initial_status);
			stmt.res_bind_all(new_initial_final_id);
			throw_assert(stmt.next()); // Previous query succeeded, so this has to
		}

		break;
	}
	}

	// Update finals
	stmt = mysql.prepare("UPDATE submissions"
		" SET contest_final=IF(id=?, 1, 0)"
		" WHERE id=?"
			" OR (owner=? AND contest_problem_id=? AND contest_final=1)");
	stmt.bindAndExecute(new_final_id, new_final_id, submission_owner,
		contest_problem_id);

	// Update initial finals
	stmt = mysql.prepare("UPDATE submissions"
		" SET contest_initial_final=IF(id=?, 1, 0)"
		" WHERE id=?"
			" OR (owner=? AND contest_problem_id=?"
			" AND contest_initial_final=1)");
	stmt.bindAndExecute(new_initial_final_id, new_initial_final_id,
		submission_owner, contest_problem_id);
}

namespace submission {

void update_final(MySQL::Connection& mysql, StringView submission_owner,
	StringView problem_id, StringView contest_problem_id, bool lock_tables)
{
	STACK_UNWINDING_MARK;

	if (submission_owner.empty()) // System submission
		return; // Nothing to do

	auto impl = [&] {
		update_problem_final(mysql, submission_owner, problem_id);
		if (not contest_problem_id.empty())
			update_contest_final(mysql, submission_owner, contest_problem_id);
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
