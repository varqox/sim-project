#include "../capabilities/jobs.hh"
#include "../capabilities/users.hh"
#include "../http/form_validation.hh"
#include "../http/response.hh"
#include "../ui_template.hh"
#include "../web_worker/context.hh"
#include "api.hh"
#include "ui.hh"

#include <cstdint>
#include <optional>
#include <sim/jobs/job.hh>
#include <sim/jobs/utils.hh>
#include <sim/users/user.hh>
#include <simlib/concat.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/json_str/json_str.hh>
#include <simlib/mysql/mysql.hh>
#include <simlib/sql.hh>
#include <simlib/string_view.hh>
#include <simlib/time.hh>
#include <utility>

using sim::jobs::Job;
using sim::users::User;
using std::optional;
using web_server::capabilities::UsersListCapabilities;
using web_server::http::Response;
using web_server::web_worker::Context;

namespace capabilities = web_server::capabilities;

namespace {

struct UserInfo {
    decltype(User::id) id{};
    decltype(User::type) type{};
    decltype(User::username) username;
    decltype(User::first_name) first_name;
    decltype(User::last_name) last_name;
    decltype(User::email) email;

    explicit UserInfo() = default;

    UserInfo(const UserInfo&) = delete;
    UserInfo(UserInfo&&) = delete;
    UserInfo& operator=(const UserInfo&) = delete;
    UserInfo& operator=(UserInfo&&) = delete;
    ~UserInfo() = default;

    void append_to(const decltype(Context::session)& session, json_str::ObjectBuilder& obj) {
        const auto caps = capabilities::user(session, id, type);
        assert(caps.view);
        obj.prop("id", id);
        obj.prop("type", type);
        obj.prop("username", username);
        obj.prop("first_name", first_name);
        obj.prop("last_name", last_name);
        obj.prop("email", email);
        obj.prop_obj("capabilities", [&](auto& obj) {
            obj.prop("view", caps.view);
            obj.prop("edit", caps.edit);
            obj.prop("edit_username", caps.edit_username);
            obj.prop("edit_first_name", caps.edit_first_name);
            obj.prop("edit_last_name", caps.edit_last_name);
            obj.prop("edit_email", caps.edit_email);
            obj.prop("change_password", caps.change_password);
            obj.prop(
                "change_password_without_old_password", caps.change_password_without_old_password
            );
            obj.prop("change_type", caps.change_type);
            obj.prop("make_admin", caps.make_admin);
            obj.prop("make_teacher", caps.make_teacher);
            obj.prop("make_normal", caps.make_normal);
            obj.prop("delete", caps.delete_);
            obj.prop("merge_into_another_user", caps.merge_into_another_user);
            obj.prop("merge_someone_into_this_user", caps.merge_someone_into_this_user);
        });
    }
};

template <class... Params>
Response do_list(Context& ctx, uint32_t limit, sql::Condition<Params...> where_cond) {
    UserInfo u;
    auto stmt = ctx.mysql.prepare_bind_and_execute(
        sql::Select("id, type, username, first_name, last_name, email")
            .from("users")
            .where(where_cond)
            .order_by("id")
            .limit("?", limit)
    );
    stmt.res_bind_all(u.id, u.type, u.username, u.first_name, u.last_name, u.email);

    json_str::Object obj;
    obj.prop("may_be_more", stmt.rows_num() == limit);
    obj.prop_arr("list", [&](auto& arr) {
        while (stmt.next()) {
            arr.val_obj([&](auto& obj) { u.append_to(ctx.session, obj); });
        }
    });
    return ctx.response_json(std::move(obj).into_str());
}

constexpr bool
is_query_allowed(UsersListCapabilities caps, optional<decltype(User::type)> user_type) noexcept {
    if (not user_type) {
        return caps.query_all;
    }
    switch (*user_type) {
    case User::Type::ADMIN: return caps.query_with_type_admin;
    case User::Type::TEACHER: return caps.query_with_type_teacher;
    case User::Type::NORMAL: return caps.query_with_type_normal;
    }
    __builtin_unreachable();
}

sql::Condition<>
caps_to_condition(UsersListCapabilities caps, optional<decltype(User::type)> user_type) {
    auto res = sql::FALSE;
    if (caps.view_all_with_type_admin and (!user_type or user_type == User::Type::ADMIN)) {
        res = res or
            sql::Condition{concat_tostr("type=", decltype(User::type){User::Type::ADMIN}.to_int())};
    }
    if (caps.view_all_with_type_teacher and (!user_type or user_type == User::Type::TEACHER)) {
        res = res or
            sql::Condition{
                concat_tostr("type=", decltype(User::type){User::Type::TEACHER}.to_int())};
    }
    if (caps.view_all_with_type_normal and (!user_type or user_type == User::Type::NORMAL)) {
        res = res or
            sql::Condition{
                concat_tostr("type=", decltype(User::type){User::Type::NORMAL}.to_int())};
    }
    return res;
}

template <class... Params>
Response do_list_all_users(
    Context& ctx,
    uint32_t limit,
    optional<decltype(User::type)> user_type,
    sql::Condition<Params...> where_cond
) {
    auto caps = capabilities::list_all_users(ctx.session);
    if (not is_query_allowed(caps, user_type)) {
        return ctx.response_403();
    }
    return do_list(ctx, limit, where_cond and caps_to_condition(caps, user_type));
}

optional<decltype(User::type)> get_user_type(Context& ctx, decltype(User::id) user_id) {
    auto stmt =
        ctx.mysql.prepare_bind_and_execute(sql::Select("type").from("users").where("id=?", user_id)
        );
    decltype(User::type) user_type;
    stmt.res_bind_all(user_type);
    if (stmt.next()) {
        return user_type;
    }
    return std::nullopt;
}

} // namespace

namespace web_server::users::api {

constexpr inline uint32_t FIRST_QUERY_LIMIT = 64;
constexpr inline uint32_t NEXT_QUERY_LIMIT = 200;

Response list_all_users(Context& ctx) {
    return do_list_all_users(ctx, FIRST_QUERY_LIMIT, std::nullopt, sql::TRUE);
}

Response list_all_users_above_id(Context& ctx, decltype(User::id) user_id) {
    return do_list_all_users(ctx, NEXT_QUERY_LIMIT, std::nullopt, sql::Condition{"id>?", user_id});
}

Response list_all_users_with_type(Context& ctx, decltype(User::type) user_type) {
    return do_list_all_users(ctx, FIRST_QUERY_LIMIT, user_type, sql::TRUE);
}

Response list_all_users_with_type_above_id(
    Context& ctx, decltype(User::type) user_type, decltype(User::id) user_id
) {
    return do_list_all_users(ctx, NEXT_QUERY_LIMIT, user_type, sql::Condition{"id>?", user_id});
}

Response view_user(Context& ctx, decltype(User::id) user_id) {
    auto user_type_opt = get_user_type(ctx, user_id);
    if (not user_type_opt) {
        return ctx.response_404();
    }
    auto caps = capabilities::user(ctx.session, user_id, *user_type_opt);
    if (not caps.view) {
        return ctx.response_403();
    }

    UserInfo u;
    u.id = user_id;
    auto stmt = ctx.mysql.prepare_bind_and_execute(
        sql::Select("type, username, first_name, last_name, email")
            .from("users")
            .where("id=?", user_id)
    );
    stmt.res_bind_all(u.type, u.username, u.first_name, u.last_name, u.email);
    if (not stmt.next()) {
        return ctx.response_404();
    }

    json_str::Object obj;
    u.append_to(ctx.session, obj);
    return ctx.response_json(std::move(obj).into_str());
}

namespace params {

constexpr http::ApiParam username{&User::username, "username", "Username"};
constexpr http::ApiParam type{&User::type, "type", "Type"};
constexpr http::ApiParam first_name{&User::first_name, "first_name", "First name"};
constexpr http::ApiParam last_name{&User::last_name, "last_name", "Last name"};
constexpr http::ApiParam email{&User::email, "email", "Email"};
constexpr http::ApiParam<CStringView> password{"password", "Password"};
constexpr http::ApiParam<CStringView> password_repeated{"password_repeated", "Password (repeat)"};
constexpr http::ApiParam<CStringView> old_password{"old_password", "Old password"};
constexpr http::ApiParam<CStringView> new_password{"new_password", "New password"};
constexpr http::ApiParam<CStringView> new_password_repeated{
    "new_password_repeated", "New password (repeat)"};
constexpr http::ApiParam<bool> remember_for_a_month{"remember_for_a_month", "Remember for a month"};
constexpr http::ApiParam target_user_id{&User::id, "target_user_id", "Target user ID"};

} // namespace params

Response sign_in(Context& ctx) {
    auto caps = capabilities::users(ctx.session);
    if (not caps.sign_in) {
        return ctx.response_403();
    }

    VALIDATE(ctx.request.form_fields, ctx.response_400,
        (username, params::username, REQUIRED)
        (password, allow_blank(params::password), REQUIRED)
        (remember_for_a_month, params::remember_for_a_month, REQUIRED)
    );

    auto stmt =
        ctx.mysql.prepare_bind_and_execute(sql::Select("id, password_salt, password_hash, type")
                                               .from("users")
                                               .where("username=?", username));
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

Response sign_up(Context& ctx) {
    auto caps = capabilities::users(ctx.session);
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
        "INSERT IGNORE INTO users(created_at, type, username, first_name, last_name, email, "
        "password_salt, password_hash) VALUES(?, ?, ?, ?, ?, ?, ?, ?)"
    );
    stmt.bind_and_execute(
        mysql_date(),
        user_type,
        username,
        first_name,
        last_name,
        email,
        password_salt,
        password_hash
    );
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

Response sign_out(Context& ctx) {
    auto caps = capabilities::users(ctx.session);
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

Response add(Context& ctx) {
    auto caps = capabilities::users(ctx.session);
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
        "INSERT IGNORE INTO users(created_at, type, username, first_name, last_name, "
        "email, password_salt, password_hash) VALUES(?, ?, ?, ?, ?, ?, ?, ?)"
    );
    stmt.bind_and_execute(mysql_date(), type, username, first_name, last_name, email, salt, hash);
    if (stmt.affected_rows() != 1) {
        return ctx.response_400("Username taken");
    }

    auto user_id = stmt.insert_id();
    stdlog("New user: {id: ", user_id, ", username: ", json_stringify(username), '}');

    return ctx.response_ok(from_unsafe{to_string(user_id)});
}

Response edit(Context& ctx, decltype(User::id) user_id) {
    auto user_type_opt = get_user_type(ctx, user_id);
    if (not user_type_opt) {
        return ctx.response_404();
    }
    auto caps = capabilities::user(ctx.session, user_id, *user_type_opt);
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

    // Check if username is taken
    if (username) {
        auto stmt = ctx.mysql.prepare("SELECT id FROM users WHERE username=? AND id!=?");
        stmt.bind_and_execute(username, user_id);
        if (stmt.next()) {
            return ctx.response_400("Username taken");
        }
    }

    auto stmt = ctx.mysql.prepare(
        "UPDATE users SET type=COALESCE(?, type), username=COALESCE(?, username), "
        "first_name=COALESCE(?, first_name), last_name=COALESCE(?, last_name), "
        "email=COALESCE(?, email) WHERE id=?"
    );
    stmt.bind_and_execute(type, username, first_name, last_name, email, user_id);

    return ctx.response_ok();
}

bool password_is_valid(mysql::Connection& mysql, decltype(User::id) user_id, StringView password) {
    auto stmt = mysql.prepare_bind_and_execute(
        sql::Select("password_salt, password_hash").from("users").where("id=?", user_id)
    );
    decltype(User::password_salt) password_salt;
    decltype(User::password_hash) password_hash;
    stmt.res_bind_all(password_salt, password_hash);
    if (not stmt.next()) {
        return false;
    }
    return sim::users::password_matches(password, password_salt, password_hash);
}

Response change_password(Context& ctx, decltype(User::id) user_id) {
    auto user_type_opt = get_user_type(ctx, user_id);
    if (not user_type_opt) {
        return ctx.response_404();
    }
    auto caps = capabilities::user(ctx.session, user_id, *user_type_opt);
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

    if (old_password and !password_is_valid(ctx.mysql, user_id, *old_password)) {
        return ctx.response_400("Invalid old password");
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

Response delete_(Context& ctx, decltype(User::id) user_id) {
    auto user_type_opt = get_user_type(ctx, user_id);
    if (not user_type_opt) {
        return ctx.response_404();
    }
    auto caps = capabilities::user(ctx.session, user_id, *user_type_opt);
    if (not caps.delete_) {
        return ctx.response_403();
    }

    VALIDATE(ctx.request.form_fields, ctx.response_400,
        (password, allow_blank(params::password), REQUIRED)
    );

    if (!password_is_valid(ctx.mysql, ctx.session.value().user_id, password)) {
        return ctx.response_400("Invalid password");
    }

    // Queue the deleting job
    auto stmt = ctx.mysql.prepare("INSERT INTO jobs (creator, type, priority, status, "
                                  "added, aux_id, info, data) VALUES(?, ?, ?, ?, ?, ?, '', '')");
    constexpr auto type = Job::Type::DELETE_USER;
    stmt.bind_and_execute(
        ctx.session.value().user_id,
        EnumVal{type},
        sim::jobs::default_priority(type),
        Job::Status::PENDING,
        mysql_date(),
        user_id
    );
    ctx.notify_job_server_after_commit = true;

    json_str::Object obj;
    obj.prop("job_id", stmt.insert_id());
    return ctx.response_json(std::move(obj).into_str());
}

Response merge_into_another(Context& ctx, decltype(User::id) user_id) {
    auto user_type_opt = get_user_type(ctx, user_id);
    if (not user_type_opt) {
        return ctx.response_404();
    }
    auto caps = capabilities::user(ctx.session, user_id, *user_type_opt);
    if (not caps.merge_into_another_user) {
        return ctx.response_403();
    }

    VALIDATE(ctx.request.form_fields, ctx.response_400,
        (target_user_id, params::target_user_id, REQUIRED)
        (password, allow_blank(params::password), REQUIRED)
    );

    if (!password_is_valid(ctx.mysql, ctx.session.value().user_id, password)) {
        return ctx.response_400("Invalid password");
    }

    if (target_user_id == user_id) {
        return ctx.response_400("You cannot merge the user with themself");
    }

    auto target_user_type_opt = get_user_type(ctx, target_user_id);
    if (not target_user_type_opt) {
        return ctx.response_404();
    }
    auto target_user_caps = capabilities::user(ctx.session, target_user_id, *target_user_type_opt);
    if (not target_user_caps.merge_someone_into_this_user) {
        return ctx.response_400("You cannot merge into this target target user");
    }

    // Queue the merging job
    auto stmt = ctx.mysql.prepare("INSERT INTO jobs (creator, type, priority, status, "
                                  "added, aux_id, info, data) VALUES(?, ?, ?, ?, ?, ?, ?, '')");
    constexpr auto type = Job::Type::MERGE_USERS;
    stmt.bind_and_execute(
        ctx.session.value().user_id,
        EnumVal{type},
        sim::jobs::default_priority(type),
        Job::Status::PENDING,
        mysql_date(),
        user_id,
        sim::jobs::MergeUsersInfo{target_user_id}.dump()
    );
    ctx.notify_job_server_after_commit = true;

    json_str::Object obj;
    obj.prop("job_id", stmt.insert_id());
    return ctx.response_json(std::move(obj).into_str());
}

} // namespace web_server::users::api
