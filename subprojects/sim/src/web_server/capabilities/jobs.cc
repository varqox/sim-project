#include "jobs.hh"
#include "utils.hh"

#include <sim/contest_users/contest_user.hh>
#include <sim/jobs/job.hh>
#include <sim/problems/problem.hh>
#include <simlib/utilities.hh>

using sim::contest_users::ContestUser;
using sim::jobs::Job;
using sim::problems::Problem;
using web_server::web_worker::Context;

namespace web_server::capabilities {

JobsCapabilities jobs(const decltype(Context::session)& session) noexcept {
    return {
        .web_ui_view = is_admin(session),
    };
}

JobsListCapabilities list_jobs(const decltype(Context::session)& session) noexcept {
    return {
        .query_all = is_admin(session),
        .query_with_status_pending = is_admin(session),
        .query_with_status_in_progress = is_admin(session),
        .query_with_status_done = is_admin(session),
        .query_with_status_failed = is_admin(session),
        .query_with_status_cancelled = is_admin(session),
    };
}

JobsListCapabilities list_problem_jobs(
    const decltype(Context::session)& session, decltype(Problem::owner_id) problem_owner_id
) noexcept {
    bool owns_problem = is_self(session, problem_owner_id);
    return {
        .query_all = is_admin(session) || owns_problem,
        .query_with_status_pending = is_admin(session) || owns_problem,
        .query_with_status_in_progress = is_admin(session) || owns_problem,
        .query_with_status_done = is_admin(session) || owns_problem,
        .query_with_status_failed = is_admin(session) || owns_problem,
        .query_with_status_cancelled = is_admin(session) || owns_problem,
    };
}

JobsListCapabilities list_submission_jobs(
    const decltype(Context::session)& session,
    decltype(Problem::owner_id) submission_problem_owner_id,
    std::optional<decltype(ContestUser::mode)> submission_contest_user_mode_of_session_user
) noexcept {
    bool owns_problem = is_self(session, submission_problem_owner_id);
    bool at_least_moderates_contest = [&] {
        if (!submission_contest_user_mode_of_session_user) {
            return false;
        }
        switch (*submission_contest_user_mode_of_session_user) {
        case ContestUser::Mode::OWNER:
        case ContestUser::Mode::MODERATOR: return true;
        case ContestUser::Mode::CONTESTANT: return false;
        }
        return false;
    }();
    return {
        .query_all = is_admin(session) || owns_problem || at_least_moderates_contest,
        .query_with_status_pending =
            is_admin(session) || owns_problem || at_least_moderates_contest,
        .query_with_status_in_progress =
            is_admin(session) || owns_problem || at_least_moderates_contest,
        .query_with_status_done = is_admin(session) || owns_problem || at_least_moderates_contest,
        .query_with_status_failed = is_admin(session) || owns_problem || at_least_moderates_contest,
        .query_with_status_cancelled =
            is_admin(session) || owns_problem || at_least_moderates_contest,
    };
}

JobCapabilities
job(const decltype(Context::session)& session,
    decltype(Job::type) job_type,
    decltype(Problem::owner_id) job_problem_owner_id,
    decltype(Problem::owner_id) job_problem_2_owner_id,
    decltype(Problem::owner_id) job_submission_problem_owner_id,
    std::optional<decltype(ContestUser::mode)> job_submission_contest_user_mode_of_session_user
) noexcept {
    switch (job_type) {
    case Job::Type::JUDGE_SUBMISSION:
    case Job::Type::REJUDGE_SUBMISSION: {
        bool owns_problem = is_self(session, job_submission_problem_owner_id);
        bool at_least_moderates_contest = [&] {
            if (!job_submission_contest_user_mode_of_session_user) {
                return false;
            }
            switch (*job_submission_contest_user_mode_of_session_user) {
            case ContestUser::Mode::OWNER:
            case ContestUser::Mode::MODERATOR: return true;
            case ContestUser::Mode::CONTESTANT: return false;
            }
            return false;
        }();
        return {
            .view = is_admin(session) || owns_problem || at_least_moderates_contest,
            .view_log = is_admin(session) || owns_problem,
            .download_attached_file = false,
            .cancel = is_admin(session),
            .restart = is_admin(session),
            .change_priority = is_admin(session),
        };
    }
    case Job::Type::ADD_PROBLEM:
    case Job::Type::REUPLOAD_PROBLEM:
    case Job::Type::EDIT_PROBLEM:
    case Job::Type::DELETE_PROBLEM:
    case Job::Type::MERGE_PROBLEMS:
    case Job::Type::CHANGE_PROBLEM_STATEMENT:
    case Job::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION: {
        bool owns_problem =
            is_self(session, job_problem_owner_id) || is_self(session, job_problem_2_owner_id);
        return {
            .view = is_admin(session) || owns_problem,
            .view_log = is_admin(session) || owns_problem,
            .download_attached_file = is_one_of(
                job_type,
                Job::Type::ADD_PROBLEM,
                Job::Type::REUPLOAD_PROBLEM,
                Job::Type::CHANGE_PROBLEM_STATEMENT
            ),
            .cancel = is_admin(session) || owns_problem,
            .restart = is_admin(session) || owns_problem,
            .change_priority = is_admin(session),
        };
    }
    case Job::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
    case Job::Type::DELETE_USER:
    case Job::Type::DELETE_CONTEST:
    case Job::Type::DELETE_CONTEST_ROUND:
    case Job::Type::DELETE_CONTEST_PROBLEM:
    case Job::Type::DELETE_INTERNAL_FILE:
    case Job::Type::MERGE_USERS: break;
    }
    return {
        .view = is_admin(session),
        .view_log = is_admin(session),
        .download_attached_file = false,
        .cancel = is_admin(session),
        .restart = is_admin(session),
        .change_priority = is_admin(session),
    };
}

} // namespace web_server::capabilities
