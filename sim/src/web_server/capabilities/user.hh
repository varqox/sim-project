#pragma once

#include "src/web_server/web_worker/context.hh"

namespace web_server::capabilities {

struct User {
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
    bool merge : 1;
};

User user_for(
    const decltype(web_worker::Context::session)& session,
    decltype(sim::users::User::id) id) noexcept;

} // namespace web_server::capabilities
