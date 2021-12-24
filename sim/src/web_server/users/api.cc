#include "src/web_server/users/api.hh"

#include "sim/users/user.hh"
#include "simlib/concat.hh"
#include "simlib/json_str/json_str.hh"
#include "simlib/string_view.hh"
#include "src/web_server/capabilities/user.hh"
#include "src/web_server/capabilities/users.hh"
#include "src/web_server/http/form_validation.hh"
#include "src/web_server/http/response.hh"
#include "src/web_server/ui_template.hh"
#include "src/web_server/web_worker/context.hh"

using sim::users::User;
using web_server::http::Response;
using web_server::web_worker::Context;

namespace {

Response do_list(Context& ctx, FilePath where_str, uint32_t limit) {
    decltype(User::id) id{};
    decltype(User::type) type;
    decltype(User::username) username;
    decltype(User::first_name) first_name;
    decltype(User::last_name) last_name;
    decltype(User::email) email;
    auto stmt = ctx.mysql.prepare(
            "SELECT id, type, username, first_name, last_name, email FROM users ", where_str,
            " ORDER BY id LIMIT ", limit);
    stmt.bind_and_execute();
    stmt.res_bind_all(id, type, username, first_name, last_name, email);

    json_str::Object obj;
    obj.prop("may_be_more", stmt.rows_num() == limit);
    obj.prop_arr("list", [&](auto& arr) {
        while (stmt.next()) {
            arr.val_obj([&](auto& obj) {
                obj.prop("id", id);
                obj.prop("type", type);
                obj.prop("username", username);
                obj.prop("first_name", first_name);
                obj.prop("last_name", last_name);
                obj.prop("email", email);
                obj.prop_obj("capabilities", [&](auto& obj) {
                    const auto caps = web_server::capabilities::user_for(ctx.session, id);
                    obj.prop("view", caps.view);
                    obj.prop("edit", caps.edit);
                    obj.prop("edit_username", caps.edit_username);
                    obj.prop("edit_first_name", caps.edit_first_name);
                    obj.prop("edit_last_name", caps.edit_last_name);
                    obj.prop("edit_email", caps.edit_email);
                    obj.prop("change_password", caps.change_password);
                    obj.prop("change_password_without_old_password",
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

namespace web_server::users::api {

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

Response list_by_type_above_id(
        Context& ctx, StringView user_type_str, decltype(User::id) user_id) {
    auto caps = capabilities::users_for(ctx.session);
    if (not caps.view_all_by_type) {
        return ctx.response_403();
    }
    if (auto opt = decltype(User::type)::from_str(user_type_str)) {
        return do_list(ctx, concat("WHERE type=", opt->to_int(), " AND id>", user_id),
                NEXT_QUERY_LIMIT);
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
        obj.prop("change_password_without_old_password",
                caps.change_password_without_old_password);
        obj.prop("change_type", caps.change_type);
        obj.prop("make_admin", caps.make_admin);
        obj.prop("make_teacher", caps.make_teacher);
        obj.prop("make_normal", caps.make_normal);
        obj.prop("delete", caps.delete_);
        obj.prop("merge", caps.merge);
    });
    return ctx.response_json(std::move(obj).into_str());
}

namespace params {

constexpr http::ApiParam username{&User::username, "username", "Username"};
constexpr http::ApiParam type{&User::type, "type", "Type"};
constexpr http::ApiParam first_name{&User::first_name, "first_name", "First name"};
constexpr http::ApiParam last_name{&User::last_name, "last_name", "Last name"};
constexpr http::ApiParam email{&User::email, "email", "Email"};
constexpr http::ApiParam<CStringView> password{"password", "Password"};
constexpr http::ApiParam<CStringView> password_repeated{
        "password_repeated", "Password (repeat)"};
constexpr http::ApiParam<CStringView> old_password{"old_password", "Old password"};
constexpr http::ApiParam<CStringView> new_password{"new_password", "New password"};
constexpr http::ApiParam<CStringView> new_password_repeated{
        "new_password_repeated", "New password (repeat)"};
constexpr http::ApiParam<bool> remember_for_a_month{
        "remember_for_a_month", "Remember for a month"};

} // namespace params

http::Response sign_in(web_worker::Context& ctx) {
    auto caps = capabilities::users_for(ctx.session);
    if (not caps.sign_in) {
        return ctx.response_403();
    }

    VALIDATE(ctx.request.form_fields, ctx.response_400,
        (username, params::username, REQUIRED)
        (password, allow_blank(params::password), REQUIRED)
        (remember_for_a_month, params::remember_for_a_month, REQUIRED)
    );

    auto stmt = ctx.mysql.prepare(
            "SELECT id, password_salt, password_hash, type FROM users WHERE username=?");
    stmt.bind_and_execute(username);
    decltype(User::id) user_id{};
    decltype(User::password_salt) password_salt;
    decltype(User::password_hash) password_hash;
    decltype(User::type) user_type;
    stmt.res_bind_all(user_id, password_salt, password_hash, user_type);
    if (stmt.next() and sim::users::password_matches(password, password_salt, password_hash)) {
        if (ctx.session) {
            ctx.destroy_session();
        }
        ctx.create_session(user_id, user_type, username, {}, remember_for_a_month);
        return ctx.response_json(sim_template_params(ctx.session));
    }
    return ctx.response_400("Invalid username or password");
}

http::Response sign_up(web_worker::Context& ctx) {
    auto caps = capabilities::users_for(ctx.session);
    if (not caps.sign_up) {
        return ctx.response_403();
    }
    VALIDATE(ctx.request.form_fields, ctx.response_400,
        (username, params::username, REQUIRED)
        (first_name, params::first_name, REQUIRED)
        (last_name, params::last_name, REQUIRED)
        (email, params::email, REQUIRED)
        (password, allow_blank(params::password), REQUIRED)
        (password_repeated, allow_blank(params::password_repeated), REQUIRED)
    );
    if (password != password_repeated) {
        return ctx.response_400("Passwords do not match");
    }

    auto [password_salt, password_hash] = sim::users::salt_and_hash_password(password);
    decltype(User::type) user_type = User::Type::NORMAL;
    auto stmt = ctx.mysql.prepare(
            "INSERT IGNORE INTO users(type, username, first_name, last_name, email, "
            "password_salt, password_hash) VALUES(?, ?, ?, ?, ?, ?, ?)");
    stmt.bind_and_execute(
            user_type, username, first_name, last_name, email, password_salt, password_hash);
    if (stmt.affected_rows() != 1) {
        return ctx.response_400("Username taken");
    }

    auto user_id = stmt.insert_id();
    stdlog("New user: {id: ", user_id, ", username: ", json_stringify(username), '}');
    if (ctx.session) {
        ctx.destroy_session();
    }
    ctx.create_session(user_id, user_type, username, {}, false);
    return ctx.response_json(sim_template_params(ctx.session));
}

http::Response sign_out(web_worker::Context& ctx) {
    auto caps = capabilities::users_for(ctx.session);
    if (not ctx.session) {
        assert(not caps.sign_out);
        return ctx.response_400("You are already signed out");
    }
    if (not caps.sign_out) {
        return ctx.response_400("You have no permission to sign out");
    }
    ctx.destroy_session();
    return ctx.response_json(sim_template_params(ctx.session));
}

http::Response add(web_worker::Context& ctx) {
    auto caps = capabilities::users_for(ctx.session);
    if (not caps.add_user) {
        return ctx.response_403();
    }

    VALIDATE(ctx.request.form_fields, ctx.response_400,
        (type, params::type, REQUIRED_ENUM_CAPS(
            (ADMIN, caps.add_admin)
            (TEACHER, caps.add_teacher)
            (NORMAL, caps.add_normal_user)
        ))
        (username, params::username, REQUIRED)
        (first_name, params::first_name, REQUIRED)
        (last_name, params::last_name, REQUIRED)
        (email, params::email, REQUIRED)
        (password, allow_blank(params::password), REQUIRED)
        (password_repeated, allow_blank(params::password_repeated), REQUIRED)
    );
    if (password != password_repeated) {
        return ctx.response_400("Passwords do not match");
    }

    auto [salt, hash] = sim::users::salt_and_hash_password(password);
    auto stmt = ctx.mysql.prepare(
            "INSERT IGNORE INTO users(type, username, first_name, last_name, "
            "email, password_salt, password_hash) VALUES(?, ?, ?, ?, ?, ?, ?)");
    stmt.bind_and_execute(type, username, first_name, last_name, email, salt, hash);
    if (stmt.affected_rows() != 1) {
        return ctx.response_400("Username taken");
    }

    auto user_id = stmt.insert_id();
    stdlog("New user: {id: ", user_id, ", username: ", json_stringify(username), '}');

    return ctx.response_ok(intentional_unsafe_cstring_view(to_string(user_id)));
}

http::Response edit(web_worker::Context& ctx, decltype(User::id) user_id) {
    auto caps = capabilities::user_for(ctx.session, user_id);
    if (not caps.edit) {
        return ctx.response_403();
    }

    VALIDATE(ctx.request.form_fields, ctx.response_400,
        (type, params::type, ALLOWED_ONLY_IF_ENUM_CAPS(caps.change_type,
            (ADMIN, caps.make_admin)
            (TEACHER, caps.make_teacher)
            (NORMAL, caps.make_normal)
        ))
        (username, params::username, ALLOWED_ONLY_IF(caps.edit_username))
        (first_name, params::first_name, ALLOWED_ONLY_IF(caps.edit_first_name))
        (last_name, params::last_name, ALLOWED_ONLY_IF(caps.edit_last_name))
        (email, params::email, ALLOWED_ONLY_IF(caps.edit_email))
    );

    auto stmt = ctx.mysql.prepare(
            "UPDATE users SET type=COALESCE(?, type), username=COALESCE(?, username), "
            "first_name=COALESCE(?, first_name), last_name=COALESCE(?, last_name), "
            "email=COALESCE(?, email) WHERE id=?");
    stmt.bind_and_execute(type, username, first_name, last_name, email, user_id);

    return ctx.response_ok();
}

http::Response change_password(web_worker::Context& ctx, decltype(User::id) user_id) {
    auto caps = capabilities::user_for(ctx.session, user_id);
    if (not caps.change_password) {
        return ctx.response_403();
    }

    VALIDATE(ctx.request.form_fields, ctx.response_400,
        (old_password, allow_blank(params::old_password), REQUIRED_AND_ALLOWED_ONLY_IF(!caps.change_password_without_old_password))
        (new_password, allow_blank(params::new_password), REQUIRED)
        (new_password_repeated, allow_blank(params::new_password_repeated), REQUIRED)
    );
    if (new_password != new_password_repeated) {
        return ctx.response_400("New passwords do not match");
    }

    if (old_password) {
        auto stmt =
                ctx.mysql.prepare("SELECT password_salt, password_hash FROM users WHERE id=?");
        stmt.bind_and_execute(user_id);
        decltype(User::password_salt) password_salt;
        decltype(User::password_hash) password_hash;
        stmt.res_bind_all(password_salt, password_hash);
        throw_assert(stmt.next());
        if (!sim::users::password_matches(*old_password, password_salt, password_hash)) {
            return ctx.response_400("Invalid old password");
        }
    }

    // Commit password change
    auto [password_salt, password_hash] = sim::users::salt_and_hash_password(new_password);
    ctx.mysql.prepare("UPDATE users SET password_salt=?, password_hash=? WHERE id=?")
            .bind_and_execute(password_salt, password_hash, user_id);
    // Remove other sessions (for security reasons)
    ctx.mysql.prepare("DELETE FROM sessions WHERE user_id=? AND id!=?")
            .bind_and_execute(user_id, ctx.session.value().id);

    return ctx.response_ok();
}

} // namespace web_server::users::api
