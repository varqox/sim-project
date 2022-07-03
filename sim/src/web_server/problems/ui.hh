#pragma once

#include "src/web_server/http/response.hh"
#include "src/web_server/web_worker/context.hh"

namespace web_server::problems::ui {

http::Response list_problems(web_worker::Context& ctx);

} // namespace web_server::problems::ui
