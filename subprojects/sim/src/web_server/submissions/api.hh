#pragma once

#include "../http/response.hh"
#include "../web_worker/context.hh"

#include <sim/contest_problems/contest_problem.hh>
#include <sim/contest_rounds/contest_round.hh>
#include <sim/contests/contest.hh>
#include <sim/problems/problem.hh>
#include <sim/submissions/submission.hh>
#include <sim/users/user.hh>

namespace web_server::submissions::api {

http::Response list_submissions(web_worker::Context& ctx);

http::Response list_submissions_below_id(
    web_worker::Context& ctx, decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_submissions_with_type_contest_problem_final(web_worker::Context& ctx);

http::Response list_submissions_with_type_contest_problem_final_below_id(
    web_worker::Context& ctx, decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_submissions_with_type_problem_final(web_worker::Context& ctx);

http::Response list_submissions_with_type_problem_final_below_id(
    web_worker::Context& ctx, decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_submissions_with_type_final(web_worker::Context& ctx);

http::Response list_submissions_with_type_final_below_id(
    web_worker::Context& ctx, decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_submissions_with_type_ignored(web_worker::Context& ctx);

http::Response list_submissions_with_type_ignored_below_id(
    web_worker::Context& ctx, decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_submissions_with_type_problem_solution(web_worker::Context& ctx);

http::Response list_submissions_with_type_problem_solution_below_id(
    web_worker::Context& ctx, decltype(sim::submissions::Submission::id) submission_id
);

http::Response
list_user_submissions(web_worker::Context& ctx, decltype(sim::users::User::id) user_id);

http::Response list_user_submissions_below_id(
    web_worker::Context& ctx,
    decltype(sim::users::User::id) user_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_user_submissions_with_type_contest_problem_final(
    web_worker::Context& ctx, decltype(sim::users::User::id) user_id
);

http::Response list_user_submissions_with_type_contest_problem_final_below_id(
    web_worker::Context& ctx,
    decltype(sim::users::User::id) user_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_user_submissions_with_type_problem_final(
    web_worker::Context& ctx, decltype(sim::users::User::id) user_id
);

http::Response list_user_submissions_with_type_problem_final_below_id(
    web_worker::Context& ctx,
    decltype(sim::users::User::id) user_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_user_submissions_with_type_final(
    web_worker::Context& ctx, decltype(sim::users::User::id) user_id
);

http::Response list_user_submissions_with_type_final_below_id(
    web_worker::Context& ctx,
    decltype(sim::users::User::id) user_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_user_submissions_with_type_ignored(
    web_worker::Context& ctx, decltype(sim::users::User::id) user_id
);

http::Response list_user_submissions_with_type_ignored_below_id(
    web_worker::Context& ctx,
    decltype(sim::users::User::id) user_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response
list_problem_submissions(web_worker::Context& ctx, decltype(sim::problems::Problem::id) problem_id);

http::Response list_problem_submissions_below_id(
    web_worker::Context& ctx,
    decltype(sim::problems::Problem::id) problem_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_problem_submissions_with_type_contest_problem_final(
    web_worker::Context& ctx, decltype(sim::problems::Problem::id) problem_id
);

http::Response list_problem_submissions_with_type_contest_problem_final_below_id(
    web_worker::Context& ctx,
    decltype(sim::problems::Problem::id) problem_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_problem_submissions_with_type_problem_final(
    web_worker::Context& ctx, decltype(sim::problems::Problem::id) problem_id
);

http::Response list_problem_submissions_with_type_problem_final_below_id(
    web_worker::Context& ctx,
    decltype(sim::problems::Problem::id) problem_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_problem_submissions_with_type_final(
    web_worker::Context& ctx, decltype(sim::problems::Problem::id) problem_id
);

http::Response list_problem_submissions_with_type_final_below_id(
    web_worker::Context& ctx,
    decltype(sim::problems::Problem::id) problem_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_problem_submissions_with_type_ignored(
    web_worker::Context& ctx, decltype(sim::problems::Problem::id) problem_id
);

http::Response list_problem_submissions_with_type_ignored_below_id(
    web_worker::Context& ctx,
    decltype(sim::problems::Problem::id) problem_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_problem_submissions_with_type_problem_solution(
    web_worker::Context& ctx, decltype(sim::problems::Problem::id) problem_id
);

http::Response list_problem_submissions_with_type_problem_solution_below_id(
    web_worker::Context& ctx,
    decltype(sim::problems::Problem::id) problem_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_problem_and_user_submissions(
    web_worker::Context& ctx,
    decltype(sim::problems::Problem::id) problem_id,
    decltype(sim::users::User::id) user_id
);

http::Response list_problem_and_user_submissions_below_id(
    web_worker::Context& ctx,
    decltype(sim::problems::Problem::id) problem_id,
    decltype(sim::users::User::id) user_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_problem_and_user_submissions_with_type_contest_problem_final(
    web_worker::Context& ctx,
    decltype(sim::problems::Problem::id) problem_id,
    decltype(sim::users::User::id) user_id
);

http::Response list_problem_and_user_submissions_with_type_contest_problem_final_below_id(
    web_worker::Context& ctx,
    decltype(sim::problems::Problem::id) problem_id,
    decltype(sim::users::User::id) user_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_problem_and_user_submissions_with_type_problem_final(
    web_worker::Context& ctx,
    decltype(sim::problems::Problem::id) problem_id,
    decltype(sim::users::User::id) user_id
);

http::Response list_problem_and_user_submissions_with_type_problem_final_below_id(
    web_worker::Context& ctx,
    decltype(sim::problems::Problem::id) problem_id,
    decltype(sim::users::User::id) user_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_problem_and_user_submissions_with_type_final(
    web_worker::Context& ctx,
    decltype(sim::problems::Problem::id) problem_id,
    decltype(sim::users::User::id) user_id
);

http::Response list_problem_and_user_submissions_with_type_final_below_id(
    web_worker::Context& ctx,
    decltype(sim::problems::Problem::id) problem_id,
    decltype(sim::users::User::id) user_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_problem_and_user_submissions_with_type_ignored(
    web_worker::Context& ctx,
    decltype(sim::problems::Problem::id) problem_id,
    decltype(sim::users::User::id) user_id
);

http::Response list_problem_and_user_submissions_with_type_ignored_below_id(
    web_worker::Context& ctx,
    decltype(sim::problems::Problem::id) problem_id,
    decltype(sim::users::User::id) user_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response
list_contest_submissions(web_worker::Context& ctx, decltype(sim::contests::Contest::id) contest_id);

http::Response list_contest_submissions_below_id(
    web_worker::Context& ctx,
    decltype(sim::contests::Contest::id) contest_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_contest_submissions_with_type_contest_problem_final(
    web_worker::Context& ctx, decltype(sim::contests::Contest::id) contest_id
);

http::Response list_contest_submissions_with_type_contest_problem_final_below_id(
    web_worker::Context& ctx,
    decltype(sim::contests::Contest::id) contest_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_contest_submissions_with_type_ignored(
    web_worker::Context& ctx, decltype(sim::contests::Contest::id) contest_id
);

http::Response list_contest_submissions_with_type_ignored_below_id(
    web_worker::Context& ctx,
    decltype(sim::contests::Contest::id) contest_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_contest_and_user_submissions(
    web_worker::Context& ctx,
    decltype(sim::contests::Contest::id) contest_id,
    decltype(sim::users::User::id) user_id
);

http::Response list_contest_and_user_submissions_below_id(
    web_worker::Context& ctx,
    decltype(sim::contests::Contest::id) contest_id,
    decltype(sim::users::User::id) user_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_contest_and_user_submissions_with_type_contest_problem_final(
    web_worker::Context& ctx,
    decltype(sim::contests::Contest::id) contest_id,
    decltype(sim::users::User::id) user_id
);

http::Response list_contest_and_user_submissions_with_type_contest_problem_final_below_id(
    web_worker::Context& ctx,
    decltype(sim::contests::Contest::id) contest_id,
    decltype(sim::users::User::id) user_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_contest_and_user_submissions_with_type_ignored(
    web_worker::Context& ctx,
    decltype(sim::contests::Contest::id) contest_id,
    decltype(sim::users::User::id) user_id
);

http::Response list_contest_and_user_submissions_with_type_ignored_below_id(
    web_worker::Context& ctx,
    decltype(sim::contests::Contest::id) contest_id,
    decltype(sim::users::User::id) user_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_contest_round_submissions(
    web_worker::Context& ctx, decltype(sim::contest_rounds::ContestRound::id) contest_round_id
);

http::Response list_contest_round_submissions_below_id(
    web_worker::Context& ctx,
    decltype(sim::contest_rounds::ContestRound::id) contest_round_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_contest_round_submissions_with_type_contest_problem_final(
    web_worker::Context& ctx, decltype(sim::contest_rounds::ContestRound::id) contest_round_id
);

http::Response list_contest_round_submissions_with_type_contest_problem_final_below_id(
    web_worker::Context& ctx,
    decltype(sim::contest_rounds::ContestRound::id) contest_round_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_contest_round_submissions_with_type_ignored(
    web_worker::Context& ctx, decltype(sim::contest_rounds::ContestRound::id) contest_round_id
);

http::Response list_contest_round_submissions_with_type_ignored_below_id(
    web_worker::Context& ctx,
    decltype(sim::contest_rounds::ContestRound::id) contest_round_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_contest_round_and_user_submissions(
    web_worker::Context& ctx,
    decltype(sim::contest_rounds::ContestRound::id) contest_round_id,
    decltype(sim::users::User::id) user_id
);

http::Response list_contest_round_and_user_submissions_below_id(
    web_worker::Context& ctx,
    decltype(sim::contest_rounds::ContestRound::id) contest_round_id,
    decltype(sim::users::User::id) user_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_contest_round_and_user_submissions_with_type_contest_problem_final(
    web_worker::Context& ctx,
    decltype(sim::contest_rounds::ContestRound::id) contest_round_id,
    decltype(sim::users::User::id) user_id
);

http::Response list_contest_round_and_user_submissions_with_type_contest_problem_final_below_id(
    web_worker::Context& ctx,
    decltype(sim::contest_rounds::ContestRound::id) contest_round_id,
    decltype(sim::users::User::id) user_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_contest_round_and_user_submissions_with_type_ignored(
    web_worker::Context& ctx,
    decltype(sim::contest_rounds::ContestRound::id) contest_round_id,
    decltype(sim::users::User::id) user_id
);

http::Response list_contest_round_and_user_submissions_with_type_ignored_below_id(
    web_worker::Context& ctx,
    decltype(sim::contest_rounds::ContestRound::id) contest_round_id,
    decltype(sim::users::User::id) user_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_contest_problem_submissions(
    web_worker::Context& ctx, decltype(sim::contest_problems::ContestProblem::id) contest_problem_id
);

http::Response list_contest_problem_submissions_below_id(
    web_worker::Context& ctx,
    decltype(sim::contest_problems::ContestProblem::id) contest_problem_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_contest_problem_submissions_with_type_contest_problem_final(
    web_worker::Context& ctx, decltype(sim::contest_problems::ContestProblem::id) contest_problem_id
);

http::Response list_contest_problem_submissions_with_type_contest_problem_final_below_id(
    web_worker::Context& ctx,
    decltype(sim::contest_problems::ContestProblem::id) contest_problem_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_contest_problem_submissions_with_type_ignored(
    web_worker::Context& ctx, decltype(sim::contest_problems::ContestProblem::id) contest_problem_id
);

http::Response list_contest_problem_submissions_with_type_ignored_below_id(
    web_worker::Context& ctx,
    decltype(sim::contest_problems::ContestProblem::id) contest_problem_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_contest_problem_and_user_submissions(
    web_worker::Context& ctx,
    decltype(sim::contest_problems::ContestProblem::id) contest_problem_id,
    decltype(sim::users::User::id) user_id
);

http::Response list_contest_problem_and_user_submissions_below_id(
    web_worker::Context& ctx,
    decltype(sim::contest_problems::ContestProblem::id) contest_problem_id,
    decltype(sim::users::User::id) user_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response list_contest_problem_and_user_submissions_with_type_contest_problem_final(
    web_worker::Context& ctx,
    decltype(sim::contest_problems::ContestProblem::id) contest_problem_id,
    decltype(sim::users::User::id) user_id
);

http::Response list_contest_problem_and_user_submissions_with_type_ignored(
    web_worker::Context& ctx,
    decltype(sim::contest_problems::ContestProblem::id) contest_problem_id,
    decltype(sim::users::User::id) user_id
);

http::Response list_contest_problem_and_user_submissions_with_type_ignored_below_id(
    web_worker::Context& ctx,
    decltype(sim::contest_problems::ContestProblem::id) contest_problem_id,
    decltype(sim::users::User::id) user_id,
    decltype(sim::submissions::Submission::id) submission_id
);

http::Response
view_submission(web_worker::Context& ctx, decltype(sim::submissions::Submission::id) submission_id);

} // namespace web_server::submissions::api
