#include "contests.hh"
#include "utils.hh"

#include <sim/users/user.hh>

namespace web_server::capabilities {

Contests contests_for(const decltype(web_worker::Context::session)& session) noexcept {
    return Contests{
        .web_ui_view = true,
        .view_all = is_admin(session),
        .view_public = true,
        .add_private = is_admin(session) or is_teacher(session),
        .add_public = is_admin(session),
    };
}

} // namespace web_server::capabilities
