#pragma once

#include "src/web_server/web_worker/context.hh"

namespace web_server::capabilities {

struct Users {
    bool web_ui_view : 1;
    bool view_all : 1;
    bool add_admin : 1;
    bool add_teacher : 1;
    bool add_normal_user : 1;
};

Users users_for(const decltype(web_worker::Context::session)& session) noexcept;

} // namespace web_server::capabilities
