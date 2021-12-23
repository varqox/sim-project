#include "src/web_server/capabilities/contests.hh"
#include "sim/users/user.hh"

using sim::users::User;

namespace web_server::capabilities {

Contests contests_for(const decltype(web_worker::Context::session)& session) noexcept {
    bool is_admin = session and session->user_type == User::Type::ADMIN;
    bool is_teacher = session and session->user_type == User::Type::TEACHER;
    return Contests{
            .web_ui_view = true,
            .view_all = is_admin,
            .view_public = true,
            .add_private = is_admin or is_teacher,
            .add_public = is_admin,
    };
}

} // namespace web_server::capabilities
