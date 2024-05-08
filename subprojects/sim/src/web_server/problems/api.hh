#pragma once

#include "../http/response.hh"
#include "../web_worker/context.hh"

#include <sim/problems/problem.hh>

namespace web_server::problems::api {

http::Response list_problems(web_server::web_worker::Context& ctx);

http::Response list_problems_below_id(
    web_server::web_worker::Context& ctx, decltype(sim::problems::Problem::id) problem_id
);

http::Response list_problems_with_type(
    web_server::web_worker::Context& ctx, decltype(sim::problems::Problem::type) problem_type
);

http::Response list_problems_with_type_below_id(
    web_server::web_worker::Context& ctx,
    decltype(sim::problems::Problem::type) problem_type,
    decltype(sim::problems::Problem::id) problem_id
);

http::Response
list_user_problems(web_server::web_worker::Context& ctx, decltype(sim::users::User::id) user_id);

http::Response list_user_problems_below_id(
    web_server::web_worker::Context& ctx,
    decltype(sim::users::User::id) user_id,
    decltype(sim::problems::Problem::id) problem_id
);

http::Response list_user_problems_with_type(
    web_server::web_worker::Context& ctx,
    decltype(sim::users::User::id) user_id,
    decltype(sim::problems::Problem::type) problem_type
);

http::Response list_user_problems_with_type_below_id(
    web_server::web_worker::Context& ctx,
    decltype(sim::users::User::id) user_id,
    decltype(sim::problems::Problem::type) problem_type,
    decltype(sim::problems::Problem::id) problem_id
);

http::Response
view_problem(web_server::web_worker::Context& ctx, decltype(sim::problems::Problem::id) problem_id);

http::Response add(web_worker::Context& ctx);

} // namespace web_server::problems::api
