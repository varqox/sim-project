#include "../capabilities/users.hh"
#include "../http/form_validation.hh"
#include "../http/response.hh"
#include "../ui_template.hh"
#include "../web_worker/context.hh"
#include "api.hh"

#include <cstdint>
#include <optional>
#include <sim/jobs/job.hh>
#include <sim/jobs/utils.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/sql.hh>
#include <sim/users/user.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/enum_to_underlying_type.hh>
#include <simlib/json_str/json_str.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/string_view.hh>
#include <simlib/time.hh>
#include <utility>

using sim::jobs::Job;
using sim::sql::Condition;
using sim::sql::DeleteFrom;
using sim::sql::InsertIgnoreInto;
using sim::sql::InsertInto;
using sim::sql::Select;
using sim::sql::Update;
using sim::users::User;
using std::optional;
using web_server::capabilities::UsersListCapabilities;
using web_server::http::ApiParam;
using web_server::http::Response;
using web_server::web_worker::Context;

namespace capabilities = web_server::capabilities;

namespace {

struct UserInfo {
    decltype(User::id) id;
    decltype(User::type) type;
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

    void append_to(json_str::ObjectBuilder& obj, const capabilities::UserCapabilities& caps) {
        throw_assert(caps.view);
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
Response do_list(Context& ctx, uint32_t limit, Condition<Params...>&& where_cond) {
    STACK_UNWINDING_MARK;

    UserInfo u;
    auto stmt = ctx.mysql.execute(Select("id, type, username, first_name, last_name, email")
                                      .from("users")
                                      .where(std::move(where_cond))
                                      .order_by("id")
                                      .limit("?", limit));
    stmt.res_bind(u.id, u.type, u.username, u.first_name, u.last_name, u.email);

    json_str::Object obj;
    size_t rows_num = 0;
    obj.prop_arr("list", [&](auto& arr) {
        while (stmt.next()) {
            ++rows_num;
            arr.val_obj([&](auto& obj) {
                u.append_to(obj, capabilities::user(ctx.session, u.id, u.type));
            });
        }
    });
    obj.prop("may_be_more", rows_num == limit);
    return ctx.response_json(std::move(obj).into_str());
}

constexpr bool
is_query_allowed(UsersListCapabilities caps, optional<decltype(User::type)> user_type) {
    if (!user_type) {
        return caps.query_all;
    }
    // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
    switch (*user_type) {
    case User::Type::ADMIN: return caps.query_with_type_admin;
    case User::Type::TEACHER: return caps.query_with_type_teacher;
    case User::Type::NORMAL: return caps.query_with_type_normal;
    }
    THROW("unexpected user type");
}

Condition<>
caps_to_condition(UsersListCapabilities caps, optional<decltype(User::type)> user_type) {
    optional<Condition<>> res;
    if (caps.view_all_with_type_admin and (!user_type || user_type == User::Type::ADMIN)) {
        res = std::move(res) ||
            optional{Condition(concat_tostr("type=", enum_to_underlying_type(User::Type::ADMIN)))};
    }
    if (caps.view_all_with_type_teacher and (!user_type || user_type == User::Type::TEACHER)) {
        res = std::move(res) ||
            optional{Condition(concat_tostr("type=", enum_to_underlying_type(User::Type::TEACHER)))
            };
    }
    if (caps.view_all_with_type_normal and (!user_type || user_type == User::Type::NORMAL)) {
        res = std::move(res) ||
            optional{Condition(concat_tostr("type=", enum_to_underlying_type(User::Type::NORMAL)))};
    }
    return std::move(res).value_or(Condition("FALSE"));
}

template <class... Params>
Response do_list_users(
    Context& ctx,
    uint32_t limit,
    optional<decltype(User::type)> user_type,
    Condition<Params...>&& where_cond
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_users(ctx.session);
    if (!is_query_allowed(caps, user_type)) {
        return ctx.response_403();
    }
    return do_list(ctx, limit, std::move(where_cond) && caps_to_condition(caps, user_type));
}

optional<decltype(User::type)> get_user_type(Context& ctx, decltype(User::id) user_id) {
    STACK_UNWINDING_MARK;

    auto stmt = ctx.mysql.execute(Select("type").from("users").where("id=?", user_id));
    decltype(User::type) user_type;
    stmt.res_bind(user_type);
    if (stmt.next()) {
        return user_type;
    }
    return std::nullopt;
}

optional<web_server::capabilities::UserCapabilities>
get_user_caps(Context& ctx, decltype(User::id) user_id) {
    STACK_UNWINDING_MARK;

    auto user_type_opt = get_user_type(ctx, user_id);
    if (user_type_opt) {
        return capabilities::user(ctx.session, user_id, *user_type_opt);
    }
    return std::nullopt;
}

namespace params {

constexpr ApiParam username{&User::username, "username", "Username"};
constexpr ApiParam type{&User::type, "type", "Type"};
constexpr ApiParam first_name{&User::first_name, "first_name", "First name"};
constexpr ApiParam last_name{&User::last_name, "last_name", "Last name"};
constexpr ApiParam email{&User::email, "email", "Email"};
constexpr ApiParam<CStringView> password{"password", "Password"};
constexpr ApiParam<CStringView> password_repeated{"password_repeated", "Password (repeat)"};
constexpr ApiParam<CStringView> old_password{"old_password", "Old password"};
constexpr ApiParam<CStringView> new_password{"new_password", "New password"};
constexpr ApiParam<CStringView> new_password_repeated{
    "new_password_repeated", "New password (repeat)"
};
constexpr ApiParam<bool> remember_for_a_month{"remember_for_a_month", "Remember for a month"};
constexpr ApiParam target_user_id{&User::id, "target_user_id", "Target user ID"};

} // namespace params

} // namespace

namespace web_server::users::api {

constexpr inline uint32_t FIRST_QUERY_LIMIT = 64;
constexpr inline uint32_t NEXT_QUERY_LIMIT = 200;

Response list_users(Context& ctx) {
    STACK_UNWINDING_MARK;
    return do_list_users(ctx, FIRST_QUERY_LIMIT, std::nullopt, Condition("TRUE"));
}

Response list_users_above_id(Context& ctx, decltype(User::id) user_id) {
    STACK_UNWINDING_MARK;
    return do_list_users(ctx, NEXT_QUERY_LIMIT, std::nullopt, Condition("id>?", user_id));
}

Response list_users_with_type(Context& ctx, decltype(User::type) user_type) {
    STACK_UNWINDING_MARK;
    return do_list_users(ctx, FIRST_QUERY_LIMIT, user_type, Condition("TRUE"));
}

Response list_users_with_type_and_above_id(
    Context& ctx, decltype(User::type) user_type, decltype(User::id) user_id
) {
    STACK_UNWINDING_MARK;
    return do_list_users(ctx, NEXT_QUERY_LIMIT, user_type, Condition("id>?", user_id));
}

Response view_user(Context& ctx, decltype(User::id) user_id) {
    STACK_UNWINDING_MARK;

    auto user_type_opt = get_user_type(ctx, user_id);
    if (!user_type_opt) {
        return ctx.response_404();
    }
    auto caps = capabilities::user(ctx.session, user_id, *user_type_opt);
    if (!caps.view) {
        return ctx.response_403();
    }

    UserInfo u;
    u.id = user_id;
    auto stmt = ctx.mysql.execute(
        Select("type, username, first_name, last_name, email").from("users").where("id=?", user_id)
    );
    stmt.res_bind(u.type, u.username, u.first_name, u.last_name, u.email);
    if (!stmt.next()) {
        return ctx.response_404();
    }

    json_str::Object obj;
    u.append_to(obj, capabilities::user(ctx.session, u.id, u.type));
    return ctx.response_json(std::move(obj).into_str());
}

Response sign_in(Context& ctx) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::users(ctx.session);
    if (!caps.sign_in) {
        return ctx.response_403();
    }

    VALIDATE(ctx.request.form_fields, ctx.response_400,
        (username, params::username, REQUIRED)
        (password, allow_blank(params::password), REQUIRED)
        (remember_for_a_month, params::remember_for_a_month, REQUIRED)
    );

    auto stmt = ctx.mysql.execute(
        Select("id, password_salt, password_hash, type").from("users").where("username=?", username)
    );
    decltype(User::id) user_id{};
    decltype(User::password_salt) password_salt;
    decltype(User::password_hash) password_hash;
    decltype(User::type) user_type;
    stmt.res_bind(user_id, password_salt, password_hash, user_type);
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
    STACK_UNWINDING_MARK;

    auto caps = capabilities::users(ctx.session);
    if (!caps.sign_up) {
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
    auto stmt = ctx.mysql.execute(
        InsertIgnoreInto("users(created_at, type, username, first_name, last_name, "
                         "email, password_salt, password_hash)")
            .values(
                "?, ?, ?, ?, ?, ?, ?, ?",
                utc_mysql_datetime(),
                user_type,
                username,
                first_name,
                last_name,
                email,
                password_salt,
                password_hash
            )
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
    STACK_UNWINDING_MARK;

    auto caps = capabilities::users(ctx.session);
    if (!ctx.session) {
        throw_assert(!caps.sign_out);
        return ctx.response_400("You are already signed out");
    }
    if (!caps.sign_out) {
        return ctx.response_400("You have no permission to sign out");
    }
    ctx.destroy_session();
    return ctx.response_json(sim_template_params(ctx.session));
}

Response add(Context& ctx) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::users(ctx.session);
    if (!caps.add_user) {
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
    auto stmt = ctx.mysql.execute(
        InsertIgnoreInto("users(created_at, type, username, first_name, last_name, "
                         "email, password_salt, password_hash)")
            .values(
                "?, ?, ?, ?, ?, ?, ?, ?",
                utc_mysql_datetime(),
                type,
                username,
                first_name,
                last_name,
                email,
                salt,
                hash
            )
    );
    if (stmt.affected_rows() != 1) {
        return ctx.response_400("Username taken");
    }

    auto user_id = stmt.insert_id();
    stdlog("New user: {id: ", user_id, ", username: ", json_stringify(username), '}');

    return ctx.response_ok(from_unsafe{to_string(user_id)});
}

Response edit(Context& ctx, decltype(User::id) user_id) {
    STACK_UNWINDING_MARK;

    auto user_type_opt = get_user_type(ctx, user_id);
    if (!user_type_opt) {
        return ctx.response_404();
    }
    auto caps = capabilities::user(ctx.session, user_id, *user_type_opt);
    if (!caps.edit) {
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
        auto stmt = ctx.mysql.execute(
            Select("1").from("users").where("username=? AND id!=?", username, user_id)
        );
        int x;
        stmt.res_bind(x);
        if (stmt.next()) {
            return ctx.response_400("Username taken");
        }
    }

    auto stmt = ctx.mysql.execute(
        Update("users")
            .set(
                "type=COALESCE(?, type), username=COALESCE(?, username), "
                "first_name=COALESCE(?, first_name), last_name=COALESCE(?, last_name), "
                "email=COALESCE(?, email)",
                type,
                username,
                first_name,
                last_name,
                email
            )
            .where("id=?", user_id)
    );
    return ctx.response_ok();
}

bool password_is_valid(
    sim::mysql::Connection& mysql, decltype(User::id) user_id, std::string_view password
) {
    STACK_UNWINDING_MARK;

    auto stmt =
        mysql.execute(Select("password_salt, password_hash").from("users").where("id=?", user_id));
    decltype(User::password_salt) password_salt;
    decltype(User::password_hash) password_hash;
    stmt.res_bind(password_salt, password_hash);
    if (!stmt.next()) {
        return false;
    }
    return sim::users::password_matches(password, password_salt, password_hash);
}

Response change_password(Context& ctx, decltype(User::id) user_id) {
    STACK_UNWINDING_MARK;

    auto caps_opt = get_user_caps(ctx, user_id);
    if (!caps_opt) {
        return ctx.response_404();
    }
    auto& caps = *caps_opt;
    if (!caps.change_password) {
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
    ctx.mysql.execute(Update("users")
                          .set("password_salt=?, password_hash=?", password_salt, password_hash)
                          .where("id=?", user_id));
    // Remove other sessions (for security reasons)
    ctx.mysql.execute(
        DeleteFrom("sessions").where("user_id=? AND id!=?", user_id, ctx.session.value().id)
    );

    return ctx.response_ok();
}

Response delete_(Context& ctx, decltype(User::id) user_id) {
    STACK_UNWINDING_MARK;

    auto caps_opt = get_user_caps(ctx, user_id);
    if (!caps_opt) {
        return ctx.response_404();
    }
    auto& caps = *caps_opt;
    if (!caps.delete_) {
        return ctx.response_403();
    }

    VALIDATE(ctx.request.form_fields, ctx.response_400,
        (password, allow_blank(params::password), REQUIRED)
    );

    if (!password_is_valid(ctx.mysql, ctx.session.value().user_id, password)) {
        return ctx.response_400("Invalid password");
    }

    // Queue the deleting job
    static constexpr auto type = Job::Type::DELETE_USER;
    auto stmt =
        ctx.mysql.execute(InsertInto("jobs (creator, type, priority, status, created_at, aux_id)")
                              .values(
                                  "?, ?, ?, ?, ?, ?",
                                  ctx.session.value().user_id,
                                  type,
                                  sim::jobs::default_priority(type),
                                  Job::Status::PENDING,
                                  utc_mysql_datetime(),
                                  user_id
                              ));
    ctx.notify_job_server_after_commit = true;

    json_str::Object obj;
    obj.prop("job_id", stmt.insert_id());
    return ctx.response_json(std::move(obj).into_str());
}

Response merge_into_another(Context& ctx, decltype(User::id) user_id) {
    STACK_UNWINDING_MARK;

    auto caps_opt = get_user_caps(ctx, user_id);
    if (!caps_opt) {
        return ctx.response_404();
    }
    auto& caps = *caps_opt;
    if (!caps.merge_into_another_user) {
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
    if (!target_user_type_opt) {
        return ctx.response_404();
    }
    auto target_user_caps = capabilities::user(ctx.session, target_user_id, *target_user_type_opt);
    if (!target_user_caps.merge_someone_into_this_user) {
        return ctx.response_400("You cannot merge into this target target user");
    }

    // Queue the merging job
    static constexpr auto type = Job::Type::MERGE_USERS;
    auto stmt = ctx.mysql.execute(
        InsertInto("jobs (creator, type, priority, status, created_at, aux_id, aux_id_2)")
            .values(
                "?, ?, ?, ?, ?, ?, ?",
                ctx.session.value().user_id,
                type,
                sim::jobs::default_priority(type),
                Job::Status::PENDING,
                utc_mysql_datetime(),
                user_id,
                target_user_id
            )
    );
    ctx.notify_job_server_after_commit = true;

    json_str::Object obj;
    obj.prop("job_id", stmt.insert_id());
    return ctx.response_json(std::move(obj).into_str());
}

} // namespace web_server::users::api
