#pragma once

#include "sim/users/user.hh"
#include "simlib/string_view.hh"
#include "src/web_server/http/response.hh"
#include "src/web_server/web_worker/context.hh"

namespace web_server::users::api {

http::Response list(web_worker::Context& ctx);

http::Response list_above_id(web_worker::Context& ctx, decltype(sim::users::User::id) user_id);

http::Response list_by_type(web_worker::Context& ctx, StringView user_type_str);

http::Response list_by_type_above_id(
    web_worker::Context& ctx, StringView user_type_str,
    decltype(sim::users::User::id) user_id);

http::Response view(web_worker::Context& ctx, decltype(sim::users::User::id) user_id);

http::Response sign_in(web_worker::Context& ctx);

http::Response sign_up(web_worker::Context& ctx);

http::Response sign_out(web_worker::Context& ctx);

} // namespace web_server::users::api
