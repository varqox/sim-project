#include <sim/contest_problems/old_contest_problem.hh>
#include <sim/old_mysql/old_mysql.hh>
#include <sim/submissions/old_submission.hh>
#include <sim/submissions/update_final.hh>
#include <simlib/time.hh>

using sim::contest_problems::OldContestProblem;
using sim::submissions::OldSubmission;

static void update_problem_final(
    old_mysql::ConnectionView& mysql, uint64_t submission_user_id, uint64_t problem_id
) {
    STACK_UNWINDING_MARK;

    // Such index: (final_candidate, user_id, problem_id, score DESC, full_status,
    // id DESC) is what we need, but MySQL does not support it, so the below
    // workaround is used to select the final submission efficiently
    auto stmt = mysql.prepare("SELECT score FROM submissions USE INDEX(final3) "
                              "WHERE final_candidate=1 AND user_id=?"
                              " AND problem_id=? "
                              "ORDER BY score DESC LIMIT 1");
    stmt.bind_and_execute(submission_user_id, problem_id);

    int64_t final_score = 0;
    stmt.res_bind_all(final_score);
    if (not stmt.next()) {
        // Unset final submissions if there are any because there are no
        // candidates now
        mysql
            .prepare("UPDATE submissions SET problem_final=0 "
                     "WHERE user_id=? AND problem_id=? AND problem_final=1")
            .bind_and_execute(submission_user_id, problem_id);
        return; // Nothing more to be done
    }

    EnumVal<OldSubmission::Status> full_status{};
    stmt = mysql.prepare("SELECT full_status "
                         "FROM submissions USE INDEX(final3) "
                         "WHERE final_candidate=1 AND user_id=? AND problem_id=?"
                         " AND score=? "
                         "ORDER BY full_status LIMIT 1");
    stmt.bind_and_execute(submission_user_id, problem_id, final_score);
    stmt.res_bind_all(full_status);
    throw_assert(stmt.next()); // Previous query succeeded, so this has to

    // Choose the new final submission
    stmt = mysql.prepare("SELECT id FROM submissions USE INDEX(final3) "
                         "WHERE final_candidate=1 AND user_id=? AND problem_id=?"
                         " AND score=? AND full_status=? "
                         "ORDER BY id DESC LIMIT 1");
    stmt.bind_and_execute(submission_user_id, problem_id, final_score, full_status);

    uint64_t new_final_id = 0;
    stmt.res_bind_all(new_final_id);
    throw_assert(stmt.next()); // Previous query succeeded, so this has to

    // Update finals
    mysql
        .prepare("UPDATE submissions SET problem_final=IF(id=?, 1, 0) "
                 "WHERE user_id=? AND problem_id=? AND (problem_final=1 OR id=?)")
        .bind_and_execute(new_final_id, submission_user_id, problem_id, new_final_id);
}

static void update_contest_final(
    old_mysql::ConnectionView& mysql, uint64_t submission_user_id, uint64_t contest_problem_id
) {
    // TODO: update the initial_final if the submission is half-judged (only
    // initial_status is set and method of choosing the final submission does not show points)
    STACK_UNWINDING_MARK;

    // Get the method of choosing the final submission and whether the score is revealed
    auto stmt = mysql.prepare("SELECT method_of_choosing_final_submission, score_revealing "
                              "FROM contest_problems WHERE id=?");
    stmt.bind_and_execute(contest_problem_id);

    decltype(OldContestProblem::method_of_choosing_final_submission
    ) method_of_choosing_final_submission;
    decltype(OldContestProblem::score_revealing) score_revealing;
    stmt.res_bind_all(method_of_choosing_final_submission, score_revealing);
    if (not stmt.next()) {
        return; // Such contest problem does not exist (probably had just
                // been deleted)
    }

    auto unset_all_finals = [&] {
        // Unset final submissions if there are any because there are no
        // candidates now
        mysql
            .prepare("UPDATE submissions "
                     "SET contest_final=0, contest_initial_final=0 "
                     "WHERE user_id=? AND contest_problem_id=?"
                     " AND (contest_final=1 OR contest_initial_final=1)")
            .bind_and_execute(submission_user_id, contest_problem_id);
    };

    uint64_t new_final_id = 0;
    uint64_t new_initial_final_id = 0;
    switch (method_of_choosing_final_submission) {
    case OldContestProblem::MethodOfChoosingFinalSubmission::LATEST_COMPILING: {
        // Choose the new final submission
        stmt = mysql.prepare("SELECT id FROM submissions USE INDEX(final1) "
                             "WHERE user_id=? AND contest_problem_id=?"
                             " AND final_candidate=1 "
                             "ORDER BY id DESC LIMIT 1");
        stmt.bind_and_execute(submission_user_id, contest_problem_id);
        stmt.res_bind_all(new_final_id);
        if (not stmt.next()) {
            // Nothing to do (no submission that may be final)
            return unset_all_finals();
        }

        new_initial_final_id = new_final_id;
        break;
    }
    case OldContestProblem::MethodOfChoosingFinalSubmission::HIGHEST_SCORE: {
        // Such index: (final_candidate, user_id, contest_problem_id, score DESC,
        // full_status, id DESC) is what we need, but MySQL does not support it,
        // so the below workaround is used to select the final submission
        // efficiently
        int64_t final_score = 0;
        stmt = mysql.prepare("SELECT score FROM submissions USE INDEX(final2) "
                             "WHERE final_candidate=1 AND user_id=?"
                             " AND contest_problem_id=? "
                             "ORDER BY score DESC LIMIT 1");
        stmt.bind_and_execute(submission_user_id, contest_problem_id);
        stmt.res_bind_all(final_score);
        if (not stmt.next()) {
            // Nothing to do (no submission that may be final)
            return unset_all_finals();
        }

        EnumVal<OldSubmission::Status> full_status{};
        stmt = mysql.prepare("SELECT full_status "
                             "FROM submissions USE INDEX(final2) "
                             "WHERE final_candidate=1 AND user_id=?"
                             " AND contest_problem_id=? AND score=? "
                             "ORDER BY full_status LIMIT 1");
        stmt.bind_and_execute(submission_user_id, contest_problem_id, final_score);
        stmt.res_bind_all(full_status);
        throw_assert(stmt.next()); // Previous query succeeded, so this has to

        // Choose the new final submission
        stmt = mysql.prepare("SELECT id FROM submissions USE INDEX(final2) "
                             "WHERE final_candidate=1 AND user_id=?"
                             " AND contest_problem_id=? AND score=?"
                             " AND full_status=? "
                             "ORDER BY id DESC LIMIT 1");
        stmt.bind_and_execute(submission_user_id, contest_problem_id, final_score, full_status);
        stmt.res_bind_all(new_final_id);
        throw_assert(stmt.next()); // Previous query succeeded, so this has to

        // Choose the new initial final submission
        switch (score_revealing) {
        case OldContestProblem::ScoreRevealing::NONE: {
            // Such index: (final_candidate, user_id, contest_problem_id,
            // initial_status, id DESC) is what we need, but MySQL does not
            // support it, so the below workaround is used to select the initial
            // final submission efficiently
            EnumVal<OldSubmission::Status> initial_status{};
            stmt = mysql.prepare("SELECT initial_status "
                                 "FROM submissions USE INDEX(initial_final2) "
                                 "WHERE final_candidate=1 AND user_id=?"
                                 " AND contest_problem_id=? "
                                 "ORDER BY initial_status LIMIT 1");
            stmt.bind_and_execute(submission_user_id, contest_problem_id);
            stmt.res_bind_all(initial_status);
            throw_assert(stmt.next()); // Previous query succeeded, so this has to

            stmt = mysql.prepare("SELECT id "
                                 "FROM submissions USE INDEX(initial_final2) "
                                 "WHERE final_candidate=1 AND user_id=?"
                                 " AND contest_problem_id=?"
                                 " AND initial_status=? "
                                 "ORDER BY id DESC LIMIT 1");
            stmt.bind_and_execute(submission_user_id, contest_problem_id, initial_status);
            stmt.res_bind_all(new_initial_final_id);
            throw_assert(stmt.next()); // Previous query succeeded, so this has to
            break;
        }

        case OldContestProblem::ScoreRevealing::ONLY_SCORE: {
            // Such index: (final_candidate, user_id, contest_problem_id,
            // score DESC, initial_status, id DESC) is what we need, but MySQL
            // does not support it, so the below workaround is used to select
            // the initial final submission efficiently
            EnumVal<OldSubmission::Status> initial_status{};
            stmt = mysql.prepare("SELECT initial_status "
                                 "FROM submissions USE INDEX(initial_final3) "
                                 "WHERE final_candidate=1 AND user_id=?"
                                 " AND contest_problem_id=? AND score=? "
                                 "ORDER BY initial_status LIMIT 1");
            stmt.bind_and_execute(submission_user_id, contest_problem_id, final_score);
            stmt.res_bind_all(initial_status);
            throw_assert(stmt.next()); // Previous query succeeded, so this has to

            stmt = mysql.prepare("SELECT id "
                                 "FROM submissions USE INDEX(initial_final3) "
                                 "WHERE final_candidate=1 AND user_id=?"
                                 " AND contest_problem_id=? AND score=?"
                                 " AND initial_status=? "
                                 "ORDER BY id DESC LIMIT 1");
            stmt.bind_and_execute(
                submission_user_id, contest_problem_id, final_score, initial_status
            );
            stmt.res_bind_all(new_initial_final_id);
            throw_assert(stmt.next()); // Previous query succeeded, so this has to
            break;
        }

        case OldContestProblem::ScoreRevealing::SCORE_AND_FULL_STATUS: {
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
                 " OR (user_id=? AND contest_problem_id=? AND contest_final=1)")
        .bind_and_execute(new_final_id, new_final_id, submission_user_id, contest_problem_id);

    // Update initial finals
    mysql
        .prepare("UPDATE submissions SET contest_initial_final=IF(id=?, 1, 0) "
                 "WHERE id=? OR (user_id=? AND contest_problem_id=?"
                 " AND contest_initial_final=1)")
        .bind_and_execute(
            new_initial_final_id, new_initial_final_id, submission_user_id, contest_problem_id
        );
}

namespace sim::submissions {

void update_final_lock(
    mysql::Connection& mysql, std::optional<uint64_t> submission_user_id, uint64_t problem_id
) {
    if (not submission_user_id.has_value()) {
        return; // update_final on System submission is no-op
    }

    // This acts as a lock that serializes updating finals
    auto old_mysql = old_mysql::ConnectionView{mysql};
    old_mysql
        .prepare("UPDATE submissions SET id=id "
                 "WHERE user_id=? AND problem_id=? ORDER BY id LIMIT 1")
        .bind_and_execute(submission_user_id, problem_id);
}

void update_final(
    sim::mysql::Connection& mysql,
    std::optional<uint64_t> submission_user_id,
    uint64_t problem_id,
    std::optional<uint64_t> contest_problem_id,
    bool make_transaction
) {
    STACK_UNWINDING_MARK;

    if (not submission_user_id.has_value()) {
        return; // Nothing to do with System submission
    }

    auto impl = [&](old_mysql::ConnectionView& old_mysql) {
        update_problem_final(old_mysql, submission_user_id.value(), problem_id);
        if (contest_problem_id.has_value()) {
            update_contest_final(old_mysql, submission_user_id.value(), contest_problem_id.value());
        }
    };

    if (make_transaction) {
        auto transaction = mysql.start_repeatable_read_transaction();
        update_final_lock(mysql, submission_user_id, problem_id);
        auto old_mysql = old_mysql::ConnectionView{mysql};
        impl(old_mysql);
        transaction.commit();
    } else {
        auto old_mysql = old_mysql::ConnectionView{mysql};
        impl(old_mysql);
    }
}

} // namespace sim::submissions
