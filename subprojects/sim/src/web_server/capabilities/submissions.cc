#include "problems.hh"
#include "submissions.hh"
#include "utils.hh"

#include <optional>
#include <sim/contest_problems/contest_problem.hh>
#include <sim/contest_rounds/contest_round.hh>
#include <sim/contest_users/contest_user.hh>
#include <sim/problems/problem.hh>
#include <sim/sql/fields/datetime.hh>
#include <sim/submissions/submission.hh>
#include <simlib/time.hh>

using sim::contest_problems::ContestProblem;
using sim::contest_rounds::ContestRound;
using sim::contest_users::ContestUser;
using sim::problems::Problem;
using sim::submissions::Submission;
using sim::users::User;
using web_server::web_worker::Context;

namespace {

bool at_least_moderates_contest(std::optional<decltype(ContestUser::mode)> contest_user_mode) {
    if (!contest_user_mode) {
        return false;
    }
    // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
    switch (*contest_user_mode) {
    case ContestUser::Mode::OWNER:
    case ContestUser::Mode::MODERATOR: return true;
    case ContestUser::Mode::CONTESTANT: return false;
    }
    return false;
}

} // namespace

namespace web_server::capabilities {

SubmissionsCapabilities submissions(const decltype(Context::session)& session) noexcept {
    return {
        .web_ui_view = session.has_value(),
    };
}

SubmissionsListCapabilities list_submissions(const decltype(Context::session)& session) noexcept {
    return {
        .query_all = is_admin(session),
        .query_with_type_final = is_admin(session),
        .query_with_type_problem_final = is_admin(session),
        .query_with_type_contest_problem_final = is_admin(session),
        .query_with_type_ignored = is_admin(session),
        .query_with_type_problem_solution = is_admin(session),
        .query_with_status_judge_error = is_admin(session),
    };
}

UserSubmissionsListCapabilities list_user_submissions(
    const decltype(Context::session)& session, decltype(User::id) user_id
) noexcept {
    return {
        .query_all = is_self(session, user_id) || is_admin(session),
        .query_with_type_final = is_admin(session),
        .query_with_type_problem_final = is_admin(session),
        .query_with_type_contest_problem_final = is_admin(session),
        .query_with_type_ignored = is_self(session, user_id) || is_admin(session),
    };
}

SubmissionsListCapabilities list_problem_submissions(
    const decltype(Context::session)& session, decltype(Problem::owner_id) problem_owner_id
) noexcept {
    bool owns_problem = problem_owner_id && is_self(session, *problem_owner_id);
    return {
        .query_all = is_admin(session) || owns_problem,
        .query_with_type_final = is_admin(session) || owns_problem,
        .query_with_type_problem_final = is_admin(session) || owns_problem,
        .query_with_type_contest_problem_final = is_admin(session) || owns_problem,
        .query_with_type_ignored = is_admin(session) || owns_problem,
        .query_with_type_problem_solution = is_admin(session) || owns_problem,
        .query_with_status_judge_error = false,
    };
}

UserSubmissionsListCapabilities list_problem_and_user_submissions(
    const decltype(Context::session)& session,
    decltype(Problem::visibility) problem_visibility,
    decltype(Problem::owner_id) problem_owner_id,
    decltype(User::id) user_id
) noexcept {
    auto list_problem_submissions_caps = list_problem_submissions(session, problem_owner_id);
    auto problem_caps = problem(session, problem_visibility, problem_owner_id);
    return {
        .query_all = list_problem_submissions_caps.query_all ||
            (problem_caps.view && is_self(session, user_id)),
        .query_with_type_final = list_problem_submissions_caps.query_with_type_final,
        .query_with_type_problem_final =
            list_problem_submissions_caps.query_with_type_problem_final ||
            (problem_caps.view && is_self(session, user_id)),
        .query_with_type_contest_problem_final =
            list_problem_submissions_caps.query_with_type_contest_problem_final,
        .query_with_type_ignored = list_problem_submissions_caps.query_with_type_ignored ||
            (problem_caps.view && is_self(session, user_id)),
    };
}

ContestSubmissionsListCapabilities list_contest_submissions(
    const decltype(Context::session)& session,
    std::optional<decltype(ContestUser::mode)> session_user_contest_user_mode
) noexcept {
    bool at_least_moderates_contest = ::at_least_moderates_contest(session_user_contest_user_mode);
    return {
        .query_all = is_admin(session) || at_least_moderates_contest,
        .query_with_type_contest_problem_final = is_admin(session) || at_least_moderates_contest,
        .query_with_type_ignored = is_admin(session) || at_least_moderates_contest,
    };
}

ContestAndUserSubmissionsListCapabilities list_contest_and_user_submissions(
    const decltype(Context::session)& session,
    std::optional<decltype(ContestUser::mode)> session_user_contest_user_mode,
    decltype(User::id) user_id
) noexcept {
    auto contest_submissions_caps =
        list_contest_submissions(session, session_user_contest_user_mode);
    return {
        .query_all = contest_submissions_caps.query_all || is_self(session, user_id),
        .query_with_type_contest_problem_final =
            contest_submissions_caps.query_with_type_contest_problem_final ||
            is_self(session, user_id),
        .query_with_type_contest_problem_final_always_show_full_results =
            contest_submissions_caps.query_with_type_contest_problem_final,
        .query_with_type_ignored =
            contest_submissions_caps.query_with_type_ignored || is_self(session, user_id),
    };
}

ContestRoundAndUserSubmissionsListCapabilities list_contest_round_and_user_submissions(
    const decltype(web_worker::Context::session)& session,
    std::optional<decltype(sim::contest_users::ContestUser::mode)> session_user_contest_user_mode,
    decltype(sim::users::User::id) user_id,
    const decltype(sim::contest_rounds::ContestRound::full_results)& full_results,
    const std::string& curr_datetime
) {
    auto contest_and_user_submissions_caps =
        list_contest_and_user_submissions(session, session_user_contest_user_mode, user_id);
    return {
        .query_all = contest_and_user_submissions_caps.query_all,
        .query_with_type_contest_problem_final =
            contest_and_user_submissions_caps.query_with_type_contest_problem_final,
        .query_with_type_contest_problem_final_show_full_instead_of_initial_results =
            contest_and_user_submissions_caps
                .query_with_type_contest_problem_final_always_show_full_results ||
            curr_datetime >= full_results,
        .query_with_type_ignored = contest_and_user_submissions_caps.query_with_type_ignored,
    };
}

SubmissionCapabilities submission(
    const decltype(Context::session)& session,
    decltype(Submission::user_id) submission_user_id,
    decltype(Submission::type) submission_type,
    decltype(Problem::visibility) submission_problem_visibility,
    decltype(Problem::owner_id) submission_problem_owner_id,
    std::optional<decltype(ContestUser::mode)> session_user_contest_user_mode,
    std::optional<decltype(ContestRound::full_results)> submission_contest_round_full_results,
    std::optional<decltype(ContestProblem::method_of_choosing_final_submission)>
        submission_contest_problem_method_of_choosing_final_submission,
    std::optional<decltype(ContestProblem::score_revealing)>
        submission_contest_problem_score_revealing,
    const std::string& curr_datetime
) noexcept {
    bool at_least_moderates_contest = ::at_least_moderates_contest(session_user_contest_user_mode);
    bool owns_submission = submission_user_id && is_self(session, *submission_user_id);
    bool owns_problem =
        submission_problem_owner_id && is_self(session, *submission_problem_owner_id);
    bool is_contest_submission = submission_contest_round_full_results.has_value();
    bool reveal_score = submission_contest_problem_score_revealing && [&] {
        // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
        switch (*submission_contest_problem_score_revealing) {
        case ContestProblem::ScoreRevealing::NONE: return false;
        case ContestProblem::ScoreRevealing::ONLY_SCORE:
        case ContestProblem::ScoreRevealing::SCORE_AND_FULL_STATUS: return true;
        }
        return false;
    }();
    bool reveal_full_status = submission_contest_problem_score_revealing && [&] {
        // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
        switch (*submission_contest_problem_score_revealing) {
        case ContestProblem::ScoreRevealing::NONE:
        case ContestProblem::ScoreRevealing::ONLY_SCORE: return false;
        case ContestProblem::ScoreRevealing::SCORE_AND_FULL_STATUS: return true;
        }
        return false;
    }();
    bool contest_problem_final_visible_before_full_results =
        submission_contest_problem_method_of_choosing_final_submission && [&] {
            // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
            switch (*submission_contest_problem_method_of_choosing_final_submission) {
            case ContestProblem::MethodOfChoosingFinalSubmission::LATEST_COMPILING: return true;
            case ContestProblem::MethodOfChoosingFinalSubmission::HIGHEST_SCORE:
                return reveal_score && reveal_full_status;
            }
            return false;
        }();
    bool may_change_type_or_delete = [&] {
        switch (submission_type) {
        case Submission::Type::NORMAL:
        case Submission::Type::IGNORED: return true;
        case Submission::Type::PROBLEM_SOLUTION: return false;
        }
        return false;
    }();

    return {
        .view = is_admin(session) || owns_problem || at_least_moderates_contest || owns_submission,
        .view_initial_status =
            is_admin(session) || owns_problem || at_least_moderates_contest || owns_submission,
        .view_full_status = is_admin(session) || owns_problem || at_least_moderates_contest ||
            (owns_submission &&
             (!is_contest_submission || reveal_full_status ||
              curr_datetime >= *submission_contest_round_full_results)),
        .view_initial_judgement_protocol =
            is_admin(session) || owns_problem || at_least_moderates_contest || owns_submission,
        .view_full_judgement_protocol = is_admin(session) || owns_problem ||
            at_least_moderates_contest ||
            (owns_submission &&
             (!is_contest_submission || curr_datetime >= *submission_contest_round_full_results)),
        .view_score = is_admin(session) || owns_problem || at_least_moderates_contest ||
            (owns_submission &&
             (!is_contest_submission || reveal_score ||
              curr_datetime >= *submission_contest_round_full_results)),
        .view_if_problem_final = is_admin(session) || owns_problem ||
            (owns_submission &&
             problem(session, submission_problem_visibility, submission_problem_owner_id).view),
        .view_if_contest_problem_initial_final = is_contest_submission &&
            (is_admin(session) || owns_problem || at_least_moderates_contest || owns_submission),
        .view_if_contest_problem_final = is_contest_submission &&
            (is_admin(session) || owns_problem || at_least_moderates_contest ||
             (owns_submission &&
              (contest_problem_final_visible_before_full_results ||
               curr_datetime >= *submission_contest_round_full_results))),
        .download =
            is_admin(session) || owns_problem || at_least_moderates_contest || owns_submission,
        .change_type = may_change_type_or_delete &&
            (is_admin(session) || owns_problem || at_least_moderates_contest),
        .rejudge = is_admin(session) || owns_problem || at_least_moderates_contest,
        .delete_ = may_change_type_or_delete &&
            (is_admin(session) || owns_problem || at_least_moderates_contest),
        .view_related_jobs = is_admin(session), // TODO: check after implementing jobs
    };
}

} // namespace web_server::capabilities
