#include "sim/is_username.hh"
#include "sim/users/user.hh"
#include "simlib/random.hh"
#include "simlib/sha.hh"
#include "simlib/string_transform.hh"
#include "src/web_server/old/sim.hh"

using sim::users::User;
using std::array;
using std::string;

namespace web_server::old {

Sim::UserPermissions Sim::users_get_overall_permissions() noexcept {
    using PERM = UserPermissions;

    if (not session.has_value()) {
        return PERM::NONE;
    }

    switch (session->user_type) {
    case User::Type::ADMIN:
        if (session->user_id == sim::users::SIM_ROOT_UID) {
            return PERM::VIEW_ALL | PERM::ADD_USER | PERM::ADD_ADMIN | PERM::ADD_TEACHER |
                PERM::ADD_NORMAL;
        } else {
            return PERM::VIEW_ALL | PERM::ADD_USER | PERM::ADD_TEACHER | PERM::ADD_NORMAL;
        }

    case User::Type::TEACHER:
    case User::Type::NORMAL: return PERM::NONE;
    }

    return PERM::NONE; // Should not happen
}

Sim::UserPermissions
Sim::users_get_permissions(decltype(User::id) user_id, User::Type utype) noexcept {
    using PERM = UserPermissions;
    constexpr UserPermissions PERM_ADMIN = PERM::VIEW | PERM::EDIT | PERM::CHANGE_PASS |
        PERM::ADMIN_CHANGE_PASS | PERM::DELETE | PERM::MERGE;

    if (not session.has_value()) {
        return PERM::NONE;
    }

    auto viewer =
        EnumVal(session->user_type).to_int() + (session->user_id != sim::users::SIM_ROOT_UID);
    if (session->user_id == user_id) {
        constexpr UserPermissions perm[4] = {
            // Sim root
            PERM::VIEW | PERM::EDIT | PERM::CHANGE_PASS | PERM::MAKE_ADMIN,
            // Admin
            PERM::VIEW | PERM::EDIT | PERM::CHANGE_PASS | PERM::MAKE_ADMIN |
                PERM::MAKE_TEACHER | PERM::MAKE_NORMAL | PERM::DELETE,
            // Teacher
            PERM::VIEW | PERM::EDIT | PERM::CHANGE_PASS | PERM::MAKE_TEACHER |
                PERM::MAKE_NORMAL | PERM::DELETE,
            // Normal
            PERM::VIEW | PERM::EDIT | PERM::CHANGE_PASS | PERM::MAKE_NORMAL | PERM::DELETE,
        };
        return perm[viewer] | users_get_overall_permissions();
    }

    auto user = EnumVal(utype).to_int() + (user_id != sim::users::SIM_ROOT_UID);
    // Permission table [ viewer ][ user ]
    constexpr UserPermissions perm[4][4] = {
        {// Sim root
         // Sim root
         PERM::VIEW | PERM::EDIT | PERM::CHANGE_PASS,
         // Admin
         PERM_ADMIN | PERM::MAKE_ADMIN | PERM::MAKE_TEACHER | PERM::MAKE_NORMAL,
         // Teacher
         PERM_ADMIN | PERM::MAKE_ADMIN | PERM::MAKE_TEACHER | PERM::MAKE_NORMAL,
         // Normal
         PERM_ADMIN | PERM::MAKE_ADMIN | PERM::MAKE_TEACHER | PERM::MAKE_NORMAL},
        {// Admin
         PERM::VIEW, // Sim root
         PERM::VIEW, // Admin
                     // Teacher
         PERM_ADMIN | PERM::MAKE_TEACHER | PERM::MAKE_NORMAL,
         // Normal
         PERM_ADMIN | PERM::MAKE_TEACHER | PERM::MAKE_NORMAL},
        {
            // Teacher
            PERM::NONE, // Sim root
            PERM::NONE, // Admin
            PERM::NONE, // Teacher
            PERM::NONE // Normal
        },
        {
            // Normal
            PERM::NONE, // Sim root
            PERM::NONE, // Admin
            PERM::NONE, // Teacher
            PERM::NONE // Normal
        }};
    return perm[viewer][user] | users_get_overall_permissions();
}

Sim::UserPermissions Sim::users_get_permissions(decltype(User::id) user_id) {
    STACK_UNWINDING_MARK;
    using PERM = UserPermissions;

    auto stmt = mysql.prepare("SELECT type FROM users WHERE id=?");
    stmt.bind_and_execute(user_id);
    decltype(User::type) utype;
    stmt.res_bind_all(utype);
    if (not stmt.next()) {
        return PERM::NONE;
    }

    return users_get_permissions(user_id, utype);
}

bool Sim::check_submitted_password(StringView password_field_name) {
    if (not session.has_value()) {
        return false;
    }

    auto stmt = mysql.prepare("SELECT password_salt, password_hash FROM users WHERE id=?");
    stmt.bind_and_execute(session->user_id);

    decltype(User::password_salt) password_salt;
    decltype(User::password_hash) passwd_hash;
    stmt.res_bind_all(password_salt, passwd_hash);
    throw_assert(stmt.next());

    return sim::users::password_matches(
        request.form_fields.get(password_field_name).value_or(""), password_salt, passwd_hash);
}

void Sim::users_handle() {
    STACK_UNWINDING_MARK;

    StringView next_arg = url_args.extract_next_arg();
    if (next_arg == "add") { // Add user
        page_template("Add user");
        append("add_user();");

    } else if (auto uid = str2num<decltype(users_uid)>(next_arg)) { // View user
        users_uid = *uid;
        return users_user();

    } else if (next_arg.empty()) { // List users
        page_template("Users");
        append("user_chooser(false, window.location.hash);");

    } else {
        return error404();
    }
}

void Sim::users_user() {
    STACK_UNWINDING_MARK;

    StringView next_arg = url_args.extract_next_arg();
    if (next_arg.empty()) {
        page_template(intentional_unsafe_string_view(concat("User ", users_uid)));
        append("view_user(false, ", users_uid, ", window.location.hash);");

    } else if (next_arg == "edit") {
        page_template(intentional_unsafe_string_view(concat("Edit user ", users_uid)));
        append("edit_user(", users_uid, ");");

    } else if (next_arg == "delete") {
        page_template(intentional_unsafe_string_view(concat("Delete user ", users_uid)));
        append("delete_user(", users_uid, ");");

    } else if (next_arg == "merge") {
        page_template(intentional_unsafe_string_view(concat("Merge user ", users_uid)));
        append("merge_user(", users_uid, ");");

    } else if (next_arg == "change-password") {
        page_template(
            intentional_unsafe_string_view(concat("Change password of the user ", users_uid)));
        append("change_user_password(", users_uid, ");");

    } else {
        return error404();
    }
}

} // namespace web_server::old
