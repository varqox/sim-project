#include "ui.hh"
#include "../http/response.hh"
#include "../web_worker/context.hh"

using web_server::http::Response;
using web_server::web_worker::Context;

namespace web_server::problems::ui {

Response list_problems(Context& ctx) { return ctx.response_ui("Problems", "list_problems()"); }

} // namespace web_server::problems::ui
