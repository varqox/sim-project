#include "src/web_server/users/ui.hh"

#include "simlib/string_view.hh"
#include "src/web_server/http/response.hh"
#include "src/web_server/web_worker/context.hh"

using sim::users::User;

namespace web_server::users::ui {

http::Response sign_in(web_worker::Context& ctx) {
    return ctx.response_ui("Sign in", "sign_in()");
}

http::Response sign_up(web_worker::Context& ctx) {
    return ctx.response_ui("Sign up", "sign_up()");
}

http::Response sign_out(web_worker::Context& ctx) {
    return ctx.response_ui("Sign out", "sign_out()");
}

http::Response edit(web_worker::Context& ctx, decltype(User::id) user_id) {
    return ctx.response_ui(intentional_unsafe_string_view(concat("Edit user ", user_id)),
            intentional_unsafe_string_view(concat("edit_user(", user_id, ')')));
}

http::Response change_password(web_worker::Context& ctx, decltype(User::id) user_id) {
    return ctx.response_ui(
            intentional_unsafe_string_view(concat("Change password of the user ", user_id)),
            intentional_unsafe_string_view(concat("change_user_password(", user_id, ')')));
}

http::Response delete_(web_worker::Context& ctx, decltype(User::id) user_id) {
    return ctx.response_ui(intentional_unsafe_string_view(concat("Delete user ", user_id)),
            intentional_unsafe_string_view(concat("delete_user(", user_id, ')')));
}

} // namespace web_server::users::ui
