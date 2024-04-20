#include "users.hh"
#include "utils.hh"

#include <sim/users/user.hh>

namespace web_server::capabilities {

UsersCapabilities users(const decltype(web_worker::Context::session)& session) noexcept {
    return {
        .web_ui_view = is_admin(session),
        .add_user = is_admin(session),
        .add_admin = session and session->user_id == sim::users::SIM_ROOT_ID,
        .add_teacher = is_admin(session),
        .add_normal_user = is_admin(session),
        .sign_in = true,
        .sign_up = true,
        .sign_out = session.has_value(),
    };
}

UsersListCapabilities list_users(const decltype(web_worker::Context::session)& session) noexcept {
    return {
        .query_all = is_admin(session),
        .query_with_type_admin = is_admin(session),
        .query_with_type_teacher = is_admin(session),
        .query_with_type_normal = is_admin(session),
        .view_all_with_type_admin = is_admin(session),
        .view_all_with_type_teacher = is_admin(session),
        .view_all_with_type_normal = is_admin(session),
    };
}

UserCapabilities user(
    const decltype(web_worker::Context::session)& session,
    decltype(sim::users::User::id) user_id,
    decltype(sim::users::User::type) user_type
) noexcept {
    using sim::users::SIM_ROOT_ID;
    using Type = sim::users::User::Type;
    bool is_admin_ = is_admin(session);
    bool is_teacher_ = is_teacher(session);
    bool is_self_ = is_self(session, user_id);
    bool edit = is_self_ or (is_admin_ and user_id != SIM_ROOT_ID);
    bool make_admin =
        (session and session->user_id == SIM_ROOT_ID) or (edit and user_type == Type::ADMIN);
    bool make_teacher =
        (user_id != SIM_ROOT_ID) and (is_admin_ or (edit and user_type == Type::TEACHER));
    bool make_normal = (user_id != SIM_ROOT_ID) and
        (is_admin_ or (is_teacher_ and is_self_) or (edit and user_type == Type::NORMAL));
    return {
        .view = is_self_ or is_admin_,
        .edit = edit,
        .edit_username = is_self_ or (is_admin_ and user_id != SIM_ROOT_ID),
        .edit_first_name = is_self_ or (is_admin_ and user_id != SIM_ROOT_ID),
        .edit_last_name = is_self_ or (is_admin_ and user_id != SIM_ROOT_ID),
        .edit_email = is_self_ or (is_admin_ and user_id != SIM_ROOT_ID),
        .change_password = is_self_ or (is_admin_ and user_id != SIM_ROOT_ID),
        .change_password_without_old_password =
            not is_self_ and is_admin_ and user_id != SIM_ROOT_ID,
        .change_type = make_admin or make_teacher or make_normal,
        .make_admin = make_admin,
        .make_teacher = make_teacher,
        .make_normal = make_normal,
        .delete_ = user_id != SIM_ROOT_ID and (is_self_ or is_admin_),
        .merge_into_another_user = user_id != SIM_ROOT_ID and is_admin_,
        .merge_someone_into_this_user = user_id != SIM_ROOT_ID
    };
}

} // namespace web_server::capabilities
