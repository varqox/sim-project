#pragma once

#include "src/web_server/http/response.hh"
#include "src/web_server/web_worker/context.hh"

namespace web_server::users::ui {

http::Response list(web_worker::Context& ctx);

http::Response sign_in(web_worker::Context& ctx);

http::Response sign_up(web_worker::Context& ctx);

http::Response sign_out(web_worker::Context& ctx);

http::Response add(web_worker::Context& ctx);

http::Response edit(web_worker::Context& ctx, decltype(sim::users::User::id) user_id);

http::Response change_password(
        web_worker::Context& ctx, decltype(sim::users::User::id) user_id);

http::Response delete_(web_worker::Context& ctx, decltype(sim::users::User::id) user_id);

} // namespace web_server::users::ui
