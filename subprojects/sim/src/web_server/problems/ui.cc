#include "../http/response.hh"
#include "../web_worker/context.hh"
#include "ui.hh"

#include <sim/problems/problem.hh>

using sim::problems::Problem;
using web_server::http::Response;
using web_server::web_worker::Context;

namespace web_server::problems::ui {

Response list_problems(Context& ctx) { return ctx.response_ui("Problems", "list_problems()"); }

Response add(Context& ctx) { return ctx.response_ui("Add problem", "add_problem()"); }

Response reupload(Context& ctx, decltype(Problem::id) problem_id) {
    return ctx.response_ui(
        from_unsafe{concat("Reupload problem ", problem_id)},
        from_unsafe{concat("reupload_problem(", problem_id, ')')}
    );
}

} // namespace web_server::problems::ui
