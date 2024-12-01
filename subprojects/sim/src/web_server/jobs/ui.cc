#include "../http/response.hh"
#include "../web_worker/context.hh"
#include "ui.hh"

using web_server::http::Response;
using web_server::web_worker::Context;

namespace web_server::jobs::ui {

Response list_jobs(Context& ctx) { return ctx.response_ui("Jobs", "list_jobs()"); }

} // namespace web_server::jobs::ui
