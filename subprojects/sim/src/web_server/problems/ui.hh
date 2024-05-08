#pragma once

#include "../http/response.hh"
#include "../web_worker/context.hh"

namespace web_server::problems::ui {

http::Response list_problems(web_worker::Context& ctx);

http::Response add(web_worker::Context& ctx);

} // namespace web_server::problems::ui
