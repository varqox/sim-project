#include <array>
#include <sim/contest_problems/contest_problem.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/sql.hh>
#include <sim/submissions/submission.hh>
#include <sim/submissions/update_final.hh>
#include <simlib/utilities.hh>

using sim::contest_problems::ContestProblem;
using sim::sql::Select;
using sim::sql::Update;
using sim::submissions::Submission;

namespace {

void update_problem_final(
    sim::mysql::Connection& mysql,
    decltype(Submission::user_id)::value_type submission_user_id,
    decltype(Submission::problem_id) submission_problem_id
) {
    // First unset the current final
    mysql.execute(Update("submissions")
                      .set("problem_final=0")
                      .where(
                          "user_id=? AND problem_id=? AND problem_final=1",
                          submission_user_id,
                          submission_problem_id
                      ));
    static_assert(
        is_sorted(std::array{
            Submission::Status::OK,
            Submission::Status::WA,
            Submission::Status::TLE,
            Submission::Status::MLE,
            Submission::Status::OLE,
            Submission::Status::RTE,
        }),
        "Needed by the query below"
    );
    decltype(Submission::id) new_problem_final_submission_id;
    auto stmt = mysql.execute(Select("id")
                                  .from("submissions USE INDEX(for_problem_final)")
                                  .where(
                                      "final_candidate=1 AND user_id=? AND problem_id=?",
                                      submission_user_id,
                                      submission_problem_id
                                  )
                                  .order_by("score DESC, full_status, id DESC")
                                  .limit("1"));
    stmt.res_bind(new_problem_final_submission_id);
    if (stmt.next()) {
        mysql.execute(Update("submissions")
                          .set("problem_final=1")
                          .where("id=?", new_problem_final_submission_id));
    }
}

void update_contest_problem_final(
    sim::mysql::Connection& mysql,
    decltype(Submission::user_id)::value_type submission_user_id,
    decltype(Submission::contest_problem_id)::value_type submission_contest_problem_id,
    decltype(ContestProblem::method_of_choosing_final_submission
    ) method_of_choosing_final_submission
) {
    // First unset the current final
    mysql.execute(Update("submissions")
                      .set("contest_problem_final=0")
                      .where(
                          "user_id=? AND contest_problem_id=? AND contest_problem_final=1",
                          submission_user_id,
                          submission_contest_problem_id
                      ));

    auto stmt = [&] {
        // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
        switch (method_of_choosing_final_submission) {
        case ContestProblem::MethodOfChoosingFinalSubmission::LATEST_COMPILING:
            return mysql.execute(Select("id")
                                     .from("submissions USE INDEX(for_contest_problem_final_latest)"
                                     )
                                     .where(
                                         "final_candidate=1 AND user_id=? AND contest_problem_id=?",
                                         submission_user_id,
                                         submission_contest_problem_id
                                     )
                                     .order_by("id DESC")
                                     .limit("1"));
        case ContestProblem::MethodOfChoosingFinalSubmission::HIGHEST_SCORE:
            return mysql.execute(
                Select("id")
                    .from("submissions USE INDEX(for_contest_problem_final_by_score)")
                    .where(
                        "final_candidate=1 AND user_id=? AND contest_problem_id=?",
                        submission_user_id,
                        submission_contest_problem_id
                    )
                    .order_by("score DESC, full_status, id DESC")
                    .limit("1")
            );
        }
        THROW("invalid method_of_choosing_final_submission");
    }();
    decltype(Submission::id) new_contest_problem_final_submission_id;
    stmt.res_bind(new_contest_problem_final_submission_id);
    if (stmt.next()) {
        mysql.execute(Update("submissions")
                          .set("contest_problem_final=1")
                          .where("id=?", new_contest_problem_final_submission_id));
    }
}

void update_contest_problem_initial_final(
    sim::mysql::Connection& mysql,
    decltype(Submission::user_id)::value_type submission_user_id,
    decltype(Submission::contest_problem_id)::value_type submission_contest_problem_id,
    decltype(ContestProblem::method_of_choosing_final_submission
    ) method_of_choosing_final_submission,
    decltype(ContestProblem::score_revealing) score_revealing
) {
    // First unset the current final
    mysql.execute(Update("submissions")
                      .set("contest_problem_initial_final=0")
                      .where(
                          "user_id=? AND contest_problem_id=? AND contest_problem_initial_final=1",
                          submission_user_id,
                          submission_contest_problem_id
                      ));

    auto stmt = [&] {
        // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
        switch (method_of_choosing_final_submission) {
        case ContestProblem::MethodOfChoosingFinalSubmission::LATEST_COMPILING:
            return mysql.execute(
                Select("id")
                    .from("submissions USE INDEX(for_contest_initial_problem_final_latest)")
                    .where(
                        "initial_final_candidate=1 AND user_id=? AND contest_problem_id=?",
                        submission_user_id,
                        submission_contest_problem_id
                    )
                    .order_by("id DESC")
                    .limit("1")
            );
        case ContestProblem::MethodOfChoosingFinalSubmission::HIGHEST_SCORE: {
            // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
            switch (score_revealing) {
            case sim::contest_problems::ContestProblem::ScoreRevealing::Enum::NONE:
                return mysql.execute(
                    Select("id")
                        .from("submissions USE "
                              "INDEX(for_contest_initial_problem_final_by_initial_status)")
                        .where(
                            "initial_final_candidate=1 AND user_id=? AND contest_problem_id=?",
                            submission_user_id,
                            submission_contest_problem_id
                        )
                        .order_by("initial_status, id DESC")
                        .limit("1")
                );
            case sim::contest_problems::ContestProblem::ScoreRevealing::Enum::ONLY_SCORE:
                return mysql.execute(
                    Select("id")
                        .from("submissions USE "
                              "INDEX(for_contest_initial_problem_final_by_score_and_initial_status)"
                        )
                        .where(
                            "initial_final_candidate=1 AND user_id=? AND contest_problem_id=?",
                            submission_user_id,
                            submission_contest_problem_id
                        )
                        .order_by("score DESC, initial_status, id DESC")
                        .limit("1")
                );
            case sim::contest_problems::ContestProblem::ScoreRevealing::Enum::SCORE_AND_FULL_STATUS:
                return mysql.execute(
                    Select("id")
                        .from("submissions USE "
                              "INDEX(for_contest_initial_problem_final_by_score_and_full_status)")
                        .where(
                            "initial_final_candidate=1 AND user_id=? AND contest_problem_id=?",
                            submission_user_id,
                            submission_contest_problem_id
                        )
                        .order_by("score DESC, full_status, id DESC")
                        .limit("1")
                );
            }
            THROW("invalid score_revealing");
        }
        }
        THROW("invalid method_of_choosing_final_submission");
    }();
    decltype(Submission::id) new_contest_problem_initial_final_submission_id;
    stmt.res_bind(new_contest_problem_initial_final_submission_id);
    if (stmt.next()) {
        mysql.execute(Update("submissions")
                          .set("contest_problem_initial_final=1")
                          .where("id=?", new_contest_problem_initial_final_submission_id));
    }
}

bool is_ok_for_final_candidate(Submission::Status status) {
    // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
    switch (status) {
    case Submission::Status::OK:
    case Submission::Status::WA:
    case Submission::Status::TLE:
    case Submission::Status::MLE:
    case Submission::Status::OLE:
    case Submission::Status::RTE: return true;
    case Submission::Status::PENDING:
    case Submission::Status::COMPILATION_ERROR:
    case Submission::Status::CHECKER_COMPILATION_ERROR:
    case Submission::Status::JUDGE_ERROR: return false;
    }
    THROW("invalid status");
}

} // namespace

namespace sim::submissions {

bool is_final_candidate(
    decltype(Submission::type) type,
    decltype(Submission::full_status) full_status,
    decltype(Submission::score) score
) {
    // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
    switch (type) {
    case Submission::Type::NORMAL: break;
    case Submission::Type::IGNORED:
    case Submission::Type::PROBLEM_SOLUTION: return false;
    }

    if (!score) {
        return false;
    }
    return is_ok_for_final_candidate(full_status);
}

bool is_initial_final_candidate(
    decltype(Submission::type) type,
    decltype(Submission::initial_status) initial_status,
    decltype(Submission::full_status) full_status,
    decltype(Submission::score) score,
    std::optional<ContestProblem::ScoreRevealing> contest_problem_score_revealing
) {
    // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
    switch (type) {
    case Submission::Type::NORMAL: break;
    case Submission::Type::IGNORED:
    case Submission::Type::PROBLEM_SOLUTION: return false;
    }

    if (!contest_problem_score_revealing) {
        // The submission is not a contest problem submission.
        return is_ok_for_final_candidate(initial_status);
    }
    // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
    switch (*contest_problem_score_revealing) {
    case ContestProblem::ScoreRevealing::NONE: return is_ok_for_final_candidate(initial_status);
    case ContestProblem::ScoreRevealing::ONLY_SCORE:
        return is_ok_for_final_candidate(initial_status) && score.has_value();
    case ContestProblem::ScoreRevealing::SCORE_AND_FULL_STATUS:
        return is_ok_for_final_candidate(full_status) && score.has_value();
    }
    THROW("invalid score revealing");
}

void update_final(
    sim::mysql::Connection& mysql,
    decltype(Submission::user_id) submission_user_id,
    decltype(Submission::problem_id) submission_problem_id,
    decltype(Submission::contest_problem_id) submission_contest_problem_id
) {
    if (!submission_user_id.has_value()) {
        return; // Such submissions do not have finals
    }
    update_problem_final(mysql, *submission_user_id, submission_problem_id);

    if (submission_contest_problem_id) {
        auto stmt = mysql.execute(Select("method_of_choosing_final_submission, score_revealing")
                                      .from("contest_problems")
                                      .where("id=?", submission_contest_problem_id));
        decltype(ContestProblem::method_of_choosing_final_submission
        ) method_of_choosing_final_submission;
        decltype(ContestProblem::score_revealing) score_revealing;
        stmt.res_bind(method_of_choosing_final_submission, score_revealing);
        throw_assert(stmt.next());

        update_contest_problem_final(
            mysql,
            *submission_user_id,
            *submission_contest_problem_id,
            method_of_choosing_final_submission
        );
        update_contest_problem_initial_final(
            mysql,
            *submission_user_id,
            *submission_contest_problem_id,
            method_of_choosing_final_submission,
            score_revealing
        );
    }
}

} // namespace sim::submissions
