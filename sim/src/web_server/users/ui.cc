#include "src/web_server/users/ui.hh"

#include "simlib/string_view.hh"
#include "src/web_server/http/response.hh"
#include "src/web_server/web_worker/context.hh"

using sim::users::User;
using web_server::http::Response;
using web_server::web_worker::Context;

namespace web_server::users::ui {

Response list_users(Context& ctx) { return ctx.response_ui("Users", "list_users()"); }

Response sign_in(Context& ctx) { return ctx.response_ui("Sign in", "sign_in()"); }

Response sign_up(Context& ctx) { return ctx.response_ui("Sign up", "sign_up()"); }

Response sign_out(Context& ctx) { return ctx.response_ui("Sign out", "sign_out()"); }

Response add(Context& ctx) { return ctx.response_ui("Add user", "add_user()"); }

Response edit(Context& ctx, decltype(User::id) user_id) {
    return ctx.response_ui(intentional_unsafe_string_view(concat("Edit user ", user_id)),
            intentional_unsafe_string_view(concat("edit_user(", user_id, ')')));
}

Response change_password(Context& ctx, decltype(User::id) user_id) {
    return ctx.response_ui(
            intentional_unsafe_string_view(concat("Change password of the user ", user_id)),
            intentional_unsafe_string_view(concat("change_user_password(", user_id, ')')));
}

Response delete_(Context& ctx, decltype(User::id) user_id) {
    return ctx.response_ui(intentional_unsafe_string_view(concat("Delete user ", user_id)),
            intentional_unsafe_string_view(concat("delete_user(", user_id, ')')));
}

Response merge_into_another(Context& ctx, decltype(User::id) user_id) {
    return ctx.response_ui(
            intentional_unsafe_string_view(concat("Merge user ", user_id, " into another")),
            intentional_unsafe_string_view(concat("merge_user(", user_id, ')')));
}

} // namespace web_server::users::ui
