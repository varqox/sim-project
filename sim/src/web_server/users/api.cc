#include "src/web_server/users/api.hh"

#include "sim/users/user.hh"
#include "simlib/concat.hh"
#include "simlib/json_str/json_str.hh"
#include "src/web_server/capabilities/user.hh"
#include "src/web_server/capabilities/users.hh"
#include "src/web_server/http/response.hh"
#include "src/web_server/web_worker/context.hh"

using sim::users::User;
using web_server::http::Response;
using web_server::web_worker::Context;

namespace {

Response do_list(Context& ctx, FilePath where_str, uint32_t limit) {
    decltype(User::id) id{};
    decltype(User::username) username;
    decltype(User::first_name) first_name;
    decltype(User::last_name) last_name;
    decltype(User::email) email;
    decltype(User::type) type;
    auto stmt = ctx.mysql.prepare(
        "SELECT id, username, first_name, last_name, email, type FROM users ", where_str,
        " ORDER BY id LIMIT ", limit);
    stmt.bind_and_execute();
    stmt.res_bind_all(id, username, first_name, last_name, email, type);

    json_str::Object obj;
    obj.prop("may_be_more", stmt.rows_num() == limit);
    obj.prop_arr("list", [&](auto& arr) {
        while (stmt.next()) {
            arr.val_obj([&](auto& obj) {
                obj.prop("id", id);
                obj.prop("username", username);
                obj.prop("first_name", first_name);
                obj.prop("last_name", last_name);
                obj.prop("email", email);
                obj.prop("type", type);
                obj.prop_obj("capabilities", [&](auto& obj) {
                    const auto caps = web_server::capabilities::user_for(ctx.session, id);
                    obj.prop("view", caps.view);
                    obj.prop("edit", caps.edit);
                    obj.prop("edit_username", caps.edit_username);
                    obj.prop("edit_first_name", caps.edit_first_name);
                    obj.prop("edit_last_name", caps.edit_last_name);
                    obj.prop("edit_email", caps.edit_email);
                    obj.prop("change_password", caps.change_password);
                    obj.prop(
                        "change_password_without_old_password",
                        caps.change_password_without_old_password);
                    obj.prop("make_admin", caps.make_admin);
                    obj.prop("make_teacher", caps.make_teacher);
                    obj.prop("make_normal", caps.make_normal);
                    obj.prop("delete", caps.delete_);
                    obj.prop("merge", caps.merge);
                });
            });
        }
    });
    return ctx.response_json(std::move(obj).into_str());
}

} // namespace

namespace web_server::users {

constexpr inline uint32_t FIRST_QUERY_LIMIT = 64;
constexpr inline uint32_t NEXT_QUERY_LIMIT = 200;

Response list(Context& ctx) {
    auto caps = capabilities::users_for(ctx.session);
    if (not caps.view_all) {
        return ctx.response_403();
    }
    return do_list(ctx, "", FIRST_QUERY_LIMIT);
}

Response list_above_id(Context& ctx, decltype(User::id) user_id) {
    auto caps = capabilities::users_for(ctx.session);
    if (not caps.view_all) {
        return ctx.response_403();
    }
    return do_list(ctx, concat("WHERE id>", user_id), NEXT_QUERY_LIMIT);
}

Response list_by_type(Context& ctx, StringView user_type_str) {
    auto caps = capabilities::users_for(ctx.session);
    if (not caps.view_all_by_type) {
        return ctx.response_403();
    }
    if (auto opt = decltype(User::type)::from_str(user_type_str)) {
        return do_list(ctx, concat("WHERE type=", opt->to_int()), FIRST_QUERY_LIMIT);
    }
    return ctx.response_400("Invalid user type");
}

Response
list_by_type_above_id(Context& ctx, StringView user_type_str, decltype(User::id) user_id) {
    auto caps = capabilities::users_for(ctx.session);
    if (not caps.view_all_by_type) {
        return ctx.response_403();
    }
    if (auto opt = decltype(User::type)::from_str(user_type_str)) {
        return do_list(
            ctx, concat("WHERE type=", opt->to_int(), " AND id>", user_id), NEXT_QUERY_LIMIT);
    }
    return ctx.response_400("Invalid user type");
}

Response view(Context& ctx, decltype(User::id) user_id) {
    const auto caps = capabilities::user_for(ctx.session, user_id);
    if (not caps.view) {
        return ctx.response_403();
    }

    decltype(User::username) username;
    decltype(User::first_name) first_name;
    decltype(User::last_name) last_name;
    decltype(User::email) email;
    decltype(User::type) type;
    auto stmt = ctx.mysql.prepare(
        "SELECT username, first_name, last_name, email, type FROM users WHERE id=?");
    stmt.bind_and_execute(user_id);
    stmt.res_bind_all(username, first_name, last_name, email, type);
    if (not stmt.next()) {
        return ctx.response_404();
    }

    json_str::Object obj;
    obj.prop("id", user_id);
    obj.prop("username", username);
    obj.prop("first_name", first_name);
    obj.prop("last_name", last_name);
    obj.prop("email", email);
    obj.prop("type", type);
    obj.prop_obj("capabilities", [&](auto& obj) {
        obj.prop("edit", caps.edit);
        obj.prop("edit_username", caps.edit_username);
        obj.prop("edit_first_name", caps.edit_first_name);
        obj.prop("edit_last_name", caps.edit_last_name);
        obj.prop("edit_email", caps.edit_email);
        obj.prop("change_password", caps.change_password);
        obj.prop(
            "change_password_without_old_password", caps.change_password_without_old_password);
        obj.prop("make_admin", caps.make_admin);
        obj.prop("make_teacher", caps.make_teacher);
        obj.prop("make_normal", caps.make_normal);
        obj.prop("delete", caps.delete_);
        obj.prop("merge", caps.merge);
    });
    return ctx.response_json(std::move(obj).into_str());
}

} // namespace web_server::users
