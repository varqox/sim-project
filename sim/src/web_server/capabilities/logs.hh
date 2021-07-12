#pragma once

#include "src/web_server/web_worker/context.hh"

namespace web_server::capabilities {

struct Logs {
    bool view : 1;
};

Logs logs_for(const decltype(web_worker::Context::session)& session) noexcept;

} // namespace web_server::capabilities
