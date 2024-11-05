#include "../http/response.hh"
#include "../web_worker/context.hh"
#include "ui.hh"

using web_server::http::Response;
using web_server::web_worker::Context;

namespace web_server::submissions::ui {

Response list_submissions(Context& ctx) {
    return ctx.response_ui("Submissions", "list_submissions()");
}

} // namespace web_server::submissions::ui
