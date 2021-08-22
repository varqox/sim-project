#include "src/web_server/users/ui.hh"

#include "src/web_server/http/response.hh"
#include "src/web_server/web_worker/context.hh"

namespace web_server::users::ui {

http::Response sign_in(web_worker::Context& ctx) {
    return ctx.response_ui("Sign in", "sign_in()");
}

http::Response sign_up(web_worker::Context& ctx) {
    return ctx.response_ui("Sign up", "sign_up()");
}

http::Response sign_out(web_worker::Context& ctx) {
    return ctx.response_ui("Sign out", "sign_out()");
}

} // namespace web_server::users::ui
