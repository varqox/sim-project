#pragma once

#include "src/web_server/web_worker/context.hh"

namespace web_server::capabilities {

struct Problems {
    bool view : 1;
    bool view_all : 1;
    bool view_with_type_contest_only : 1;
    bool view_with_type_public : 1;
    bool select_by_owner : 1;
    bool add_with_type_private : 1;
    bool add_with_type_contest_only : 1;
    bool add_with_type_public : 1;
};

Problems problems_for(const decltype(web_worker::Context::session)& session) noexcept;

} // namespace web_server::capabilities
