#include "sim/users/user.hh"
#include "src/web_server/capabilities/user.hh"
#include "src/web_server/web_worker/context.hh"

namespace web_server::capabilities {

User user_for(const decltype(web_worker::Context::session)& session,
        decltype(sim::users::User::id) id) noexcept {
    using sim::users::SIM_ROOT_UID;
    bool is_admin = session and session->user_type == sim::users::User::Type::ADMIN;
    bool is_teacher = session and session->user_type == sim::users::User::Type::TEACHER;
    bool is_self = session and session->user_id == id;
    bool make_admin = session and session->user_id == SIM_ROOT_UID;
    bool make_teacher = (id != SIM_ROOT_UID) and is_admin;
    bool make_normal = (id != SIM_ROOT_UID) and (is_admin or (is_teacher and is_self));
    return User{
            .view = is_self or is_admin,
            .edit = is_self or (is_admin and id != SIM_ROOT_UID),
            .edit_username = is_self or (is_admin and id != SIM_ROOT_UID),
            .edit_first_name = is_self or (is_admin and id != SIM_ROOT_UID),
            .edit_last_name = is_self or (is_admin and id != SIM_ROOT_UID),
            .edit_email = is_self or (is_admin and id != SIM_ROOT_UID),
            .change_password = is_self or (is_admin and id != SIM_ROOT_UID),
            .change_password_without_old_password =
                    not is_self and is_admin and id != SIM_ROOT_UID,
            .change_type = make_admin or make_teacher or make_normal,
            .make_admin = make_admin,
            .make_teacher = make_teacher,
            .make_normal = make_normal,
            .delete_ = id != SIM_ROOT_UID and (is_self or is_admin),
            .merge = id != SIM_ROOT_UID and is_admin,
    };
}

} // namespace web_server::capabilities
