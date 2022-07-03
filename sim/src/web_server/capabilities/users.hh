#pragma once

#include "src/web_server/web_worker/context.hh"

namespace web_server::capabilities {

struct UsersCapabilities {
    bool web_ui_view : 1;
    bool add_user : 1;
    bool add_admin : 1;
    bool add_teacher : 1;
    bool add_normal_user : 1;
    bool sign_in : 1;
    bool sign_up : 1;
    bool sign_out : 1;
};

UsersCapabilities users(const decltype(web_worker::Context::session)& session) noexcept;

struct UsersListCapabilities {
    bool query_all : 1;
    bool query_with_type_admin : 1;
    bool query_with_type_teacher : 1;
    bool query_with_type_normal : 1;
    bool view_all_with_type_admin : 1;
    bool view_all_with_type_teacher : 1;
    bool view_all_with_type_normal : 1;
};

UsersListCapabilities list_all_users(
        const decltype(web_worker::Context::session)& session) noexcept;

struct UserCapabilities {
    bool view : 1;
    bool edit : 1;
    bool edit_username : 1;
    bool edit_first_name : 1;
    bool edit_last_name : 1;
    bool edit_email : 1;
    bool change_password : 1;
    bool change_password_without_old_password : 1;
    bool change_type : 1;
    bool make_admin : 1;
    bool make_teacher : 1;
    bool make_normal : 1;
    bool delete_ : 1;
    bool merge_into_another_user : 1;
    bool merge_someone_into_this_user : 1;
};

UserCapabilities user(const decltype(web_worker::Context::session)& session,
        decltype(sim::users::User::id) user_id) noexcept;

} // namespace web_server::capabilities
