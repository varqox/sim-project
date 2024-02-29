#include "../http/response.hh"
#include "../web_worker/context.hh"
#include "ui.hh"

#include <simlib/string_view.hh>

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
    return ctx.response_ui(
        from_unsafe{concat("Edit user ", user_id)}, from_unsafe{concat("edit_user(", user_id, ')')}
    );
}

Response change_password(Context& ctx, decltype(User::id) user_id) {
    return ctx.response_ui(
        from_unsafe{concat("Change password of the user ", user_id)},
        from_unsafe{concat("change_user_password(", user_id, ')')}
    );
}

Response delete_(Context& ctx, decltype(User::id) user_id) {
    return ctx.response_ui(
        from_unsafe{concat("Delete user ", user_id)},
        from_unsafe{concat("delete_user(", user_id, ')')}
    );
}

Response merge_into_another(Context& ctx, decltype(User::id) user_id) {
    return ctx.response_ui(
        from_unsafe{concat("Merge user ", user_id, " into another")},
        from_unsafe{concat("merge_user(", user_id, ')')}
    );
}

} // namespace web_server::users::ui
