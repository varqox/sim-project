#include "src/web_server/capabilities/users.hh"
#include "sim/users/user.hh"

using sim::users::User;

namespace web_server::capabilities {

Users users_for(const decltype(web_worker::Context::session)& session) noexcept {
    bool is_admin = session and session->user_type == User::Type::ADMIN;
    return Users{
        .web_ui_view = is_admin,
        .view_all = is_admin,
        .view_all_by_type = is_admin,
        .add_admin = session and session->user_id == sim::users::SIM_ROOT_UID,
        .add_teacher = is_admin,
        .add_normal_user = is_admin,
        .sign_in = true,
        .sign_up = true,
        .sign_out = session.has_value(),
    };
}

} // namespace web_server::capabilities
