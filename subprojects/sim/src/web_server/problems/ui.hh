#pragma once

#include "../http/response.hh"
#include "../web_worker/context.hh"

#include <sim/problems/problem.hh>

namespace web_server::problems::ui {

http::Response list_problems(web_worker::Context& ctx);

http::Response add(web_worker::Context& ctx);

http::Response reupload(web_worker::Context& ctx, decltype(sim::problems::Problem::id) problem_id);

} // namespace web_server::problems::ui
