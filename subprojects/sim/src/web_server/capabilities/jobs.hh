#pragma once

#include "../web_worker/context.hh"

#include <sim/contest_users/contest_user.hh>
#include <sim/jobs/job.hh>
#include <sim/problems/problem.hh>

namespace web_server::capabilities {

struct JobsCapabilities {
    bool web_ui_view : 1;
};

JobsCapabilities jobs(const decltype(web_worker::Context::session)& session) noexcept;

struct JobsListCapabilities {
    bool query_all : 1;
    bool query_with_status_pending : 1;
    bool query_with_status_in_progress : 1;
    bool query_with_status_done : 1;
    bool query_with_status_failed : 1;
    bool query_with_status_cancelled : 1;
};

JobsListCapabilities list_jobs(const decltype(web_worker::Context::session)& session) noexcept;

constexpr inline auto list_user_jobs = list_jobs;

JobsListCapabilities list_problem_jobs(
    const decltype(web_worker::Context::session)& session,
    decltype(sim::problems::Problem::owner_id) problem_owner_id
) noexcept;

JobsListCapabilities list_submission_jobs(
    const decltype(web_worker::Context::session)& session,
    decltype(sim::problems::Problem::owner_id) submission_problem_owner_id,
    std::optional<decltype(sim::contest_users::ContestUser::mode)>
        submission_contest_user_mode_of_session_user
) noexcept;

struct JobCapabilities {
    bool view : 1;
    bool view_log : 1;
    bool download_attached_file : 1;
    bool cancel : 1;
    bool restart : 1;
    bool change_priority : 1;
};

JobCapabilities
job(const decltype(web_worker::Context::session)& session,
    decltype(sim::jobs::Job::type) job_type,
    decltype(sim::problems::Problem::owner_id) job_problem_owner_id,
    decltype(sim::problems::Problem::owner_id) job_problem_2_owner_id,
    decltype(sim::problems::Problem::owner_id) job_submission_problem_owner_id,
    std::optional<decltype(sim::contest_users::ContestUser::mode)>
        job_submission_contest_user_mode_of_session_user) noexcept;

} // namespace web_server::capabilities
