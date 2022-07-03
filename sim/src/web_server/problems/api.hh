#pragma once

#include "sim/problems/problem.hh"
#include "src/web_server/http/response.hh"
#include "src/web_server/web_worker/context.hh"

namespace web_server::problems::api {

http::Response list_all_problems(web_server::web_worker::Context& ctx);

http::Response list_all_problems_below_id(
        web_server::web_worker::Context& ctx, decltype(sim::problems::Problem::id) problem_id);

http::Response list_all_problems_with_type(web_server::web_worker::Context& ctx,
        decltype(sim::problems::Problem::type) problem_type);

http::Response list_all_problems_with_type_below_id(web_server::web_worker::Context& ctx,
        decltype(sim::problems::Problem::type) problem_type,
        decltype(sim::problems::Problem::id) problem_id);

http::Response list_user_problems(
        web_server::web_worker::Context& ctx, decltype(sim::users::User::id) user_id);

http::Response list_user_problems_below_id(web_server::web_worker::Context& ctx,
        decltype(sim::users::User::id) user_id,
        decltype(sim::problems::Problem::id) problem_id);

http::Response list_user_problems_with_type(web_server::web_worker::Context& ctx,
        decltype(sim::users::User::id) user_id,
        decltype(sim::problems::Problem::type) problem_type);

http::Response list_user_problems_with_type_below_id(web_server::web_worker::Context& ctx,
        decltype(sim::users::User::id) user_id,
        decltype(sim::problems::Problem::type) problem_type,
        decltype(sim::problems::Problem::id) problem_id);

http::Response view_problem(
        web_server::web_worker::Context& ctx, decltype(sim::problems::Problem::id) problem_id);

} // namespace web_server::problems::api
