#include "src/web_server/capabilities/users.hh"
#include "sim/users/user.hh"
#include "src/web_server/capabilities/utils.hh"

namespace web_server::capabilities {

UsersCapabilities users_for(const decltype(web_worker::Context::session)& session) noexcept {
    return UsersCapabilities{
            .web_ui_view = is_admin(session),
            .view_all = is_admin(session),
            .view_all_by_type = is_admin(session),
            .add_user = is_admin(session),
            .add_admin = session and session->user_id == sim::users::SIM_ROOT_UID,
            .add_teacher = is_admin(session),
            .add_normal_user = is_admin(session),
            .sign_in = true,
            .sign_up = true,
            .sign_out = session.has_value(),
    };
}

UserCapabilities user_for(const decltype(web_worker::Context::session)& session,
        decltype(sim::users::User::id) user_id) noexcept {
    using sim::users::SIM_ROOT_UID;
    bool is_admin_ = is_admin(session);
    bool is_teacher_ = is_teacher(session);
    bool is_self_ = is_self(session, user_id);
    bool make_admin = session and session->user_id == SIM_ROOT_UID;
    bool make_teacher = (user_id != SIM_ROOT_UID) and is_admin_;
    bool make_normal = (user_id != SIM_ROOT_UID) and (is_admin_ or (is_teacher_ and is_self_));
    return UserCapabilities{.view = is_self_ or is_admin_,
            .edit = is_self_ or (is_admin_ and user_id != SIM_ROOT_UID),
            .edit_username = is_self_ or (is_admin_ and user_id != SIM_ROOT_UID),
            .edit_first_name = is_self_ or (is_admin_ and user_id != SIM_ROOT_UID),
            .edit_last_name = is_self_ or (is_admin_ and user_id != SIM_ROOT_UID),
            .edit_email = is_self_ or (is_admin_ and user_id != SIM_ROOT_UID),
            .change_password = is_self_ or (is_admin_ and user_id != SIM_ROOT_UID),
            .change_password_without_old_password =
                    not is_self_ and is_admin_ and user_id != SIM_ROOT_UID,
            .change_type = make_admin or make_teacher or make_normal,
            .make_admin = make_admin,
            .make_teacher = make_teacher,
            .make_normal = make_normal,
            .delete_ = user_id != SIM_ROOT_UID and (is_self_ or is_admin_),
            .merge_into_another_user = user_id != SIM_ROOT_UID and is_admin_,
            .merge_someone_into_this_user = user_id != SIM_ROOT_UID};
}

} // namespace web_server::capabilities
