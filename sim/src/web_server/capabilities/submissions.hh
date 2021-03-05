#pragma once

#include "src/web_server/web_worker/context.hh"

namespace web_server::capabilities {

struct Submissions {
    bool view : 1;
    bool view_all : 1;
};

Submissions submissions_for(const decltype(web_worker::Context::session)& session) noexcept;

} // namespace web_server::capabilities
