#pragma once

#include "../web_worker/context.hh"

#include <sim/contest_problems/contest_problem.hh>
#include <sim/contest_rounds/contest_round.hh>
#include <sim/contest_users/contest_user.hh>
#include <sim/problems/problem.hh>
#include <sim/submissions/submission.hh>
#include <sim/users/user.hh>
#include <simlib/time.hh>

namespace web_server::capabilities {

struct SubmissionsCapabilities {
    bool web_ui_view : 1;
};

SubmissionsCapabilities submissions(const decltype(web_worker::Context::session)& session) noexcept;

struct SubmissionsListCapabilities {
    bool query_all : 1;
    bool query_with_type_final : 1;
    bool query_with_type_problem_final : 1;
    bool query_with_type_contest_problem_final : 1;
    bool query_with_type_ignored : 1;
    bool query_with_type_problem_solution : 1;
};

SubmissionsListCapabilities list_submissions(const decltype(web_worker::Context::session)& session
) noexcept;

struct UserSubmissionsListCapabilities {
    bool query_all : 1;
    bool query_with_type_final : 1;
    bool query_with_type_problem_final : 1;
    bool query_with_type_contest_problem_final : 1;
    bool query_with_type_ignored : 1;
};

UserSubmissionsListCapabilities list_user_submissions(
    const decltype(web_worker::Context::session)& session, decltype(sim::users::User::id) user_id
) noexcept;

SubmissionsListCapabilities list_problem_submissions(
    const decltype(web_worker::Context::session)& session,
    decltype(sim::problems::Problem::owner_id) problem_owner_id
) noexcept;

UserSubmissionsListCapabilities list_problem_and_user_submissions(
    const decltype(web_worker::Context::session)& session,
    decltype(sim::problems::Problem::visibility) problem_visibility,
    decltype(sim::problems::Problem::owner_id) problem_owner_id,
    decltype(sim::users::User::id) user_id
) noexcept;

struct ContestSubmissionsListCapabilities {
    bool query_all : 1;
    bool query_with_type_contest_problem_final : 1;
    bool query_with_type_ignored : 1;
};

ContestSubmissionsListCapabilities list_contest_submissions(
    const decltype(web_worker::Context::session)& session,
    std::optional<decltype(sim::contest_users::ContestUser::mode)> session_user_contest_user_mode
) noexcept;

struct ContestAndUserSubmissionsListCapabilities {
    bool query_all : 1;
    bool query_with_type_contest_problem_final : 1;
    bool query_with_type_contest_problem_final_always_show_full_results : 1;
    bool query_with_type_ignored : 1;
};

ContestAndUserSubmissionsListCapabilities list_contest_and_user_submissions(
    const decltype(web_worker::Context::session)& session,
    std::optional<decltype(sim::contest_users::ContestUser::mode)> session_user_contest_user_mode,
    decltype(sim::users::User::id) user_id
) noexcept;

struct ContestRoundAndUserSubmissionsListCapabilities {
    bool query_all : 1;
    bool query_with_type_contest_problem_final : 1;
    bool query_with_type_contest_problem_final_show_full_instead_of_initial_results : 1;
    bool query_with_type_ignored : 1;
};

ContestRoundAndUserSubmissionsListCapabilities list_contest_round_and_user_submissions(
    const decltype(web_worker::Context::session)& session,
    std::optional<decltype(sim::contest_users::ContestUser::mode)> session_user_contest_user_mode,
    decltype(sim::users::User::id) user_id,
    const decltype(sim::contest_rounds::ContestRound::full_results)& full_results,
    const std::string& curr_datetime
);

constexpr inline auto list_contest_round_submissions = list_contest_submissions;

constexpr inline auto list_contest_problem_submissions = list_contest_submissions;

constexpr inline auto list_contest_problem_and_user_submissions =
    list_contest_round_and_user_submissions;

struct SubmissionCapabilities {
    bool view : 1;
    bool view_initial_status : 1;
    bool view_full_status : 1;
    bool view_initial_judgement_protocol : 1;
    bool view_full_judgement_protocol : 1;
    bool view_score : 1;
    bool view_if_problem_final : 1;
    bool view_if_contest_problem_initial_final : 1;
    bool view_if_contest_problem_final : 1;
    bool download : 1;
    bool change_type : 1;
    bool rejudge : 1;
    bool delete_ : 1;
    bool view_related_jobs : 1;
};

SubmissionCapabilities submission(
    const decltype(web_worker::Context::session)& session,
    decltype(sim::submissions::Submission::user_id) submission_user_id,
    decltype(sim::submissions::Submission::type) submission_type,
    decltype(sim::problems::Problem::visibility) submission_problem_visibility,
    decltype(sim::problems::Problem::owner_id) submission_problem_owner_id,
    std::optional<decltype(sim::contest_users::ContestUser::mode)> session_user_contest_user_mode,
    std::optional<decltype(sim::contest_rounds::ContestRound::full_results)>
        submission_contest_round_full_results,
    std::optional<
        decltype(sim::contest_problems::ContestProblem::method_of_choosing_final_submission)>
        submission_contest_problem_method_of_choosing_final_submission,
    std::optional<decltype(sim::contest_problems::ContestProblem::score_revealing)>
        submission_contest_problem_score_revealing,
    const std::string& curr_datetime
) noexcept;

} // namespace web_server::capabilities
