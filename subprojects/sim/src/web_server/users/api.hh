#pragma once

#include "../http/response.hh"
#include "../web_worker/context.hh"

#include <sim/mysql/mysql.hh>
#include <sim/users/user.hh>
#include <string_view>

namespace web_server::users::api {

http::Response list_users(web_worker::Context& ctx);

http::Response
list_users_above_id(web_worker::Context& ctx, decltype(sim::users::User::id) user_id);

http::Response
list_users_with_type(web_worker::Context& ctx, decltype(sim::users::User::type) user_type);

http::Response list_users_with_type_and_above_id(
    web_worker::Context& ctx,
    decltype(sim::users::User::type) user_type,
    decltype(sim::users::User::id) user_id
);

http::Response view_user(web_worker::Context& ctx, decltype(sim::users::User::id) user_id);

http::Response sign_in(web_worker::Context& ctx);

http::Response sign_up(web_worker::Context& ctx);

http::Response sign_out(web_worker::Context& ctx);

http::Response add(web_worker::Context& ctx);

http::Response edit(web_worker::Context& ctx, decltype(sim::users::User::id) user_id);

bool password_is_valid(
    sim::mysql::Connection& mysql, decltype(sim::users::User::id) user_id, std::string_view password
);

http::Response change_password(web_worker::Context& ctx, decltype(sim::users::User::id) user_id);

http::Response delete_(web_worker::Context& ctx, decltype(sim::users::User::id) user_id);

http::Response merge_into_another(web_worker::Context& ctx, decltype(sim::users::User::id) user_id);

} // namespace web_server::users::api
