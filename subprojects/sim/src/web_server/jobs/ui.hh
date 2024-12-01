#pragma once

#include "../http/response.hh"
#include "../web_worker/context.hh"

namespace web_server::jobs::ui {

http::Response list_jobs(web_worker::Context& ctx);

} // namespace web_server::jobs::ui
