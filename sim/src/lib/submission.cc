#include <sim/constants.h>
#include <sim/contest_problem.hh>
#include <sim/submission.h>
#include <simlib/time.hh>

using sim::ContestProblem;

static void update_problem_final(MySQL::Connection& mysql,
                                 uint64_t submission_owner,
                                 uint64_t problem_id) {
	STACK_UNWINDING_MARK;

	// Such index: (final_candidate, owner, problem_id, score DESC, full_status,
	// id DESC) is what we need, but MySQL does not support it, so the below
	// workaround is used to select the final submission efficiently
	auto stmt = mysql.prepare("SELECT score FROM submissions USE INDEX(final3) "
	                          "WHERE final_candidate=1 AND owner=?"
	                          " AND problem_id=? "
	                          "ORDER BY score DESC LIMIT 1");
	stmt.bind_and_execute(submission_owner, problem_id);

	int64_t final_score;
	stmt.res_bind_all(final_score);
	if (not stmt.next()) {
		// Unset final submissions if there are any because there are no
		// candidates now
		mysql
		   .prepare("UPDATE submissions SET problem_final=0 "
		            "WHERE owner=? AND problem_id=? AND problem_final=1")
		   .bind_and_execute(submission_owner, problem_id);
		return; // Nothing more to be done
	}

	EnumVal<SubmissionStatus> full_status;
	stmt = mysql.prepare("SELECT full_status "
	                     "FROM submissions USE INDEX(final3) "
	                     "WHERE final_candidate=1 AND owner=? AND problem_id=?"
	                     " AND score=? "
	                     "ORDER BY full_status LIMIT 1");
	stmt.bind_and_execute(submission_owner, problem_id, final_score);
	stmt.res_bind_all(full_status);
	throw_assert(stmt.next()); // Previous query succeeded, so this has to

	// Choose the new final submission
	stmt = mysql.prepare("SELECT id FROM submissions USE INDEX(final3) "
	                     "WHERE final_candidate=1 AND owner=? AND problem_id=?"
	                     " AND score=? AND full_status=? "
	                     "ORDER BY id DESC LIMIT 1");
	stmt.bind_and_execute(submission_owner, problem_id, final_score,
	                      full_status);

	uint64_t new_final_id;
	stmt.res_bind_all(new_final_id);
	throw_assert(stmt.next()); // Previous query succeeded, so this has to

	// Update finals
	mysql
	   .prepare("UPDATE submissions SET problem_final=IF(id=?, 1, 0) "
	            "WHERE owner=? AND problem_id=? AND (problem_final=1 OR id=?)")
	   .bind_and_execute(new_final_id, submission_owner, problem_id,
	                     new_final_id);
}

static void update_contest_final(MySQL::Connection& mysql,
                                 uint64_t submission_owner,
                                 uint64_t contest_problem_id) {
	// TODO: update the initial_final if the submission is half-judged (only
	// initial_status is set and final selecting method does not show points)
	STACK_UNWINDING_MARK;

	// Get the final selecting method and whether the score is revealed
	auto stmt = mysql.prepare("SELECT final_selecting_method, score_revealing "
	                          "FROM contest_problems WHERE id=?");
	stmt.bind_and_execute(contest_problem_id);

	decltype(ContestProblem::final_selecting_method) final_selecting_method;
	decltype(ContestProblem::score_revealing) score_revealing;
	stmt.res_bind_all(final_selecting_method, score_revealing);
	if (not stmt.next())
		return; // Such contest problem does not exist (probably had just
		        // been deleted)

	auto unset_all_finals = [&] {
		// Unset final submissions if there are any because there are no
		// candidates now
		mysql
		   .prepare("UPDATE submissions "
		            "SET contest_final=0, contest_initial_final=0 "
		            "WHERE owner=? AND contest_problem_id=?"
		            " AND (contest_final=1 OR contest_initial_final=1)")
		   .bind_and_execute(submission_owner, contest_problem_id);
	};

	uint64_t new_final_id, new_initial_final_id;
	using FSSM = ContestProblem::FinalSubmissionSelectingMethod;
	switch (final_selecting_method) {
	case FSSM::LAST_COMPILING: {
		// Choose the new final submission
		stmt = mysql.prepare("SELECT id FROM submissions USE INDEX(final1) "
		                     "WHERE owner=? AND contest_problem_id=?"
		                     " AND final_candidate=1 "
		                     "ORDER BY id DESC LIMIT 1");
		stmt.bind_and_execute(submission_owner, contest_problem_id);
		stmt.res_bind_all(new_final_id);
		if (not stmt.next()) // Nothing to do (no submission that may be final)
			return unset_all_finals();

		new_initial_final_id = new_final_id;
		break;
	}
	case FSSM::WITH_HIGHEST_SCORE: {
		// Such index: (final_candidate, owner, contest_problem_id, score DESC,
		// full_status, id DESC) is what we need, but MySQL does not support it,
		// so the below workaround is used to select the final submission
		// efficiently
		int64_t final_score;
		stmt = mysql.prepare("SELECT score FROM submissions USE INDEX(final2) "
		                     "WHERE final_candidate=1 AND owner=?"
		                     " AND contest_problem_id=? "
		                     "ORDER BY score DESC LIMIT 1");
		stmt.bind_and_execute(submission_owner, contest_problem_id);
		stmt.res_bind_all(final_score);
		if (not stmt.next()) // Nothing to do (no submission that may be final)
			return unset_all_finals();

		EnumVal<SubmissionStatus> full_status;
		stmt = mysql.prepare("SELECT full_status "
		                     "FROM submissions USE INDEX(final2) "
		                     "WHERE final_candidate=1 AND owner=?"
		                     " AND contest_problem_id=? AND score=? "
		                     "ORDER BY full_status LIMIT 1");
		stmt.bind_and_execute(submission_owner, contest_problem_id,
		                      final_score);
		stmt.res_bind_all(full_status);
		throw_assert(stmt.next()); // Previous query succeeded, so this has to

		// Choose the new final submission
		stmt = mysql.prepare("SELECT id FROM submissions USE INDEX(final2) "
		                     "WHERE final_candidate=1 AND owner=?"
		                     " AND contest_problem_id=? AND score=?"
		                     " AND full_status=? "
		                     "ORDER BY id DESC LIMIT 1");
		stmt.bind_and_execute(submission_owner, contest_problem_id, final_score,
		                      full_status);
		stmt.res_bind_all(new_final_id);
		throw_assert(stmt.next()); // Previous query succeeded, so this has to

		// Choose the new initial final submission
		switch (score_revealing) {
		case ContestProblem::ScoreRevealingMode::NONE: {
			// Such index: (final_candidate, owner, contest_problem_id,
			// initial_status, id DESC) is what we need, but MySQL does not
			// support it, so the below workaround is used to select the initial
			// final submission efficiently
			EnumVal<SubmissionStatus> initial_status;
			stmt = mysql.prepare("SELECT initial_status "
			                     "FROM submissions USE INDEX(initial_final2) "
			                     "WHERE final_candidate=1 AND owner=?"
			                     " AND contest_problem_id=? "
			                     "ORDER BY initial_status LIMIT 1");
			stmt.bind_and_execute(submission_owner, contest_problem_id);
			stmt.res_bind_all(initial_status);
			throw_assert(
			   stmt.next()); // Previous query succeeded, so this has to

			stmt = mysql.prepare("SELECT id "
			                     "FROM submissions USE INDEX(initial_final2) "
			                     "WHERE final_candidate=1 AND owner=?"
			                     " AND contest_problem_id=?"
			                     " AND initial_status=? "
			                     "ORDER BY id DESC LIMIT 1");
			stmt.bind_and_execute(submission_owner, contest_problem_id,
			                      initial_status);
			stmt.res_bind_all(new_initial_final_id);
			throw_assert(
			   stmt.next()); // Previous query succeeded, so this has to
			break;
		}

		case ContestProblem::ScoreRevealingMode::ONLY_SCORE: {
			// Such index: (final_candidate, owner, contest_problem_id,
			// score DESC, initial_status, id DESC) is what we need, but MySQL
			// does not support it, so the below workaround is used to select
			// the initial final submission efficiently
			EnumVal<SubmissionStatus> initial_status;
			stmt = mysql.prepare("SELECT initial_status "
			                     "FROM submissions USE INDEX(initial_final3) "
			                     "WHERE final_candidate=1 AND owner=?"
			                     " AND contest_problem_id=? AND score=? "
			                     "ORDER BY initial_status LIMIT 1");
			stmt.bind_and_execute(submission_owner, contest_problem_id,
			                      final_score);
			stmt.res_bind_all(initial_status);
			throw_assert(
			   stmt.next()); // Previous query succeeded, so this has to

			stmt = mysql.prepare("SELECT id "
			                     "FROM submissions USE INDEX(initial_final3) "
			                     "WHERE final_candidate=1 AND owner=?"
			                     " AND contest_problem_id=? AND score=?"
			                     " AND initial_status=? "
			                     "ORDER BY id DESC LIMIT 1");
			stmt.bind_and_execute(submission_owner, contest_problem_id,
			                      final_score, initial_status);
			stmt.res_bind_all(new_initial_final_id);
			throw_assert(
			   stmt.next()); // Previous query succeeded, so this has to
			break;
		}

		case ContestProblem::ScoreRevealingMode::SCORE_AND_FULL_STATUS: {
			new_initial_final_id = new_final_id;
			break;
		}
		}

		break;
	}
	}

	// Update finals
	mysql
	   .prepare("UPDATE submissions SET contest_final=IF(id=?, 1, 0) "
	            "WHERE id=?"
	            " OR (owner=? AND contest_problem_id=? AND contest_final=1)")
	   .bind_and_execute(new_final_id, new_final_id, submission_owner,
	                     contest_problem_id);

	// Update initial finals
	mysql
	   .prepare("UPDATE submissions SET contest_initial_final=IF(id=?, 1, 0) "
	            "WHERE id=? OR (owner=? AND contest_problem_id=?"
	            " AND contest_initial_final=1)")
	   .bind_and_execute(new_initial_final_id, new_initial_final_id,
	                     submission_owner, contest_problem_id);
}

namespace submission {

void update_final_lock(MySQL::Connection& mysql,
                       std::optional<uint64_t> submission_owner,
                       uint64_t problem_id) {
	if (not submission_owner.has_value())
		return; // update_final on System submission is no-op

	// This acts as a lock that serializes updating finals
	mysql
	   .prepare("UPDATE submissions SET id=id "
	            "WHERE owner=? AND problem_id=? ORDER BY id LIMIT 1")
	   .bind_and_execute(submission_owner, problem_id);
}

void update_final(MySQL::Connection& mysql,
                  std::optional<uint64_t> submission_owner, uint64_t problem_id,
                  std::optional<uint64_t> contest_problem_id,
                  bool make_transaction) {
	STACK_UNWINDING_MARK;

	if (not submission_owner.has_value())
		return; // Nothing to do with System submission

	auto impl = [&] {
		update_problem_final(mysql, submission_owner.value(), problem_id);
		if (contest_problem_id.has_value())
			update_contest_final(mysql, submission_owner.value(),
			                     contest_problem_id.value());
	};

	if (make_transaction) {
		auto transaction = mysql.start_transaction();
		update_final_lock(mysql, submission_owner, problem_id);
		impl();
		transaction.commit();
	} else {
		impl();
	}
}

} // namespace submission
