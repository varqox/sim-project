#pragma once

#include "src/web_server/http/response.hh"
#include "src/web_server/web_worker/context.hh"

namespace web_server::users::ui {

http::Response sign_in(web_worker::Context& ctx);

http::Response sign_up(web_worker::Context& ctx);

http::Response sign_out(web_worker::Context& ctx);

} // namespace web_server::users::ui
