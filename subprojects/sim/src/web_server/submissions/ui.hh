#pragma once

#include "../http/response.hh"
#include "../web_worker/context.hh"

namespace web_server::submissions::ui {

http::Response list_submissions(web_worker::Context& ctx);

} // namespace web_server::submissions::ui
