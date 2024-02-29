#pragma once

#include "../web_worker/context.hh"

namespace web_server::capabilities {

struct Contests {
    bool web_ui_view : 1;
    bool view_all : 1;
    bool view_public : 1;
    bool add_private : 1;
    bool add_public : 1;
};

Contests contests_for(const decltype(web_worker::Context::session)& session) noexcept;

} // namespace web_server::capabilities
