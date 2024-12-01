#pragma once

#include "../http/response.hh"
#include "../web_worker/context.hh"

#include <sim/jobs/job.hh>
#include <sim/problems/problem.hh>
#include <sim/submissions/submission.hh>
#include <sim/users/user.hh>

namespace web_server::jobs::api {

http::Response list_jobs(web_worker::Context& ctx);

http::Response list_jobs_below_id(web_worker::Context& ctx, decltype(sim::jobs::Job::id) job_id);

http::Response
list_jobs_with_status(web_worker::Context& ctx, decltype(sim::jobs::Job::status) job_status);

http::Response list_jobs_with_status_below_id(
    web_worker::Context& ctx,
    decltype(sim::jobs::Job::status) job_status,
    decltype(sim::jobs::Job::id) job_id
);

http::Response list_user_jobs(web_worker::Context& ctx, decltype(sim::users::User::id) user_id);

http::Response list_user_jobs_below_id(
    web_worker::Context& ctx,
    decltype(sim::users::User::id) user_id,
    decltype(sim::jobs::Job::id) job_id
);

http::Response list_user_jobs_with_status(
    web_worker::Context& ctx,
    decltype(sim::users::User::id) user_id,
    decltype(sim::jobs::Job::status) job_status
);

http::Response list_user_jobs_with_status_below_id(
    web_worker::Context& ctx,
    decltype(sim::users::User::id) user_id,
    decltype(sim::jobs::Job::status) job_status,
    decltype(sim::jobs::Job::id) job_id
);

http::Response
list_problem_jobs(web_worker::Context& ctx, decltype(sim::problems::Problem::id) problem_id);

http::Response list_problem_jobs_below_id(
    web_worker::Context& ctx,
    decltype(sim::problems::Problem::id) problem_id,
    decltype(sim::jobs::Job::id) job_id
);

http::Response list_problem_jobs_with_status(
    web_worker::Context& ctx,
    decltype(sim::problems::Problem::id) problem_id,
    decltype(sim::jobs::Job::status) job_status
);

http::Response list_problem_jobs_with_status_below_id(
    web_worker::Context& ctx,
    decltype(sim::problems::Problem::id) problem_id,
    decltype(sim::jobs::Job::status) job_status,
    decltype(sim::jobs::Job::id) job_id
);

http::Response list_submission_jobs(
    web_worker::Context& ctx, decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_submission_jobs_below_id(
    web_worker::Context& ctx,
    decltype(sim::submissions::Submission::id) submission_id,
    decltype(sim::jobs::Job::id) job_id
);

http::Response list_submission_jobs_with_status(
    web_worker::Context& ctx,
    decltype(sim::submissions::Submission::id) submission_id,
    decltype(sim::jobs::Job::status) job_status
);

http::Response list_submission_jobs_with_status_below_id(
    web_worker::Context& ctx,
    decltype(sim::submissions::Submission::id) submission_id,
    decltype(sim::jobs::Job::status) job_status,
    decltype(sim::jobs::Job::id) job_id
);

http::Response view_job(web_worker::Context& ctx, decltype(sim::jobs::Job::id) job_id);

} // namespace web_server::jobs::api
