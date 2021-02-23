#include "sim/constants.hh"
#include "sim/user.hh"
#include "sim/utilities.hh"
#include "simlib/random.hh"
#include "simlib/sha.hh"
#include "simlib/string_transform.hh"
#include "src/web_interface/sim.hh"

using sim::User;
using std::array;
using std::string;

Sim::UserPermissions Sim::users_get_overall_permissions() noexcept {
    using PERM = UserPermissions;

    if (not session_is_open) {
        return PERM::NONE;
    }

    switch (session_user_type) {
    case User::Type::ADMIN:
        if (session_user_id == sim::SIM_ROOT_UID) {
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

    if (not session_is_open) {
        return PERM::NONE;
    }

    auto viewer =
        EnumVal(session_user_type).int_val() + (session_user_id != sim::SIM_ROOT_UID);
    if (session_user_id == user_id) {
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

    auto user = EnumVal(utype).int_val() + (user_id != sim::SIM_ROOT_UID);
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
    if (not session_is_open) {
        return false;
    }

    auto stmt = mysql.prepare("SELECT salt, password FROM users WHERE id=?");
    stmt.bind_and_execute(session_user_id);

    decltype(User::salt) salt;
    decltype(User::password) passwd_hash;
    stmt.res_bind_all(salt, passwd_hash);
    throw_assert(stmt.next());

    return slow_equal(
        intentional_unsafe_string_view(sha3_512(intentional_unsafe_string_view(
            concat(salt, request.form_fields.get_or(password_field_name, ""))))),
        passwd_hash);
}

void Sim::login() {
    STACK_UNWINDING_MARK;

    decltype(User::username) username;
    bool remember = false;
    if (request.method == server::HttpRequest::POST) {
        // Try to login
        InplaceBuff<4096> password;
        // Validate all fields
        form_validate_not_blank(
            username, "username", "Username", is_username,
            "Username can only consist of characters [a-zA-Z0-9_-]",
            decltype(User::username)::max_len);

        form_validate(password, "password", "Password");

        remember = request.form_fields.contains("persistent-login");

        if (not notifications.size) {
            STACK_UNWINDING_MARK;

            auto stmt = mysql.prepare("SELECT id, salt, password FROM users WHERE username=?");
            stmt.bind_and_execute(username);

            InplaceBuff<32> uid;
            decltype(User::salt) salt;
            decltype(User::password) passwd_hash;
            stmt.res_bind_all(uid, salt, passwd_hash);

            if (stmt.next() and
                slow_equal(
                    intentional_unsafe_string_view(
                        sha3_512(intentional_unsafe_string_view(concat(salt, password)))),
                    passwd_hash))
            {
                // Delete old session
                if (session_is_open) {
                    session_destroy();
                }

                // Create new
                session_create_and_open(uid, not remember);

                // If there is a redirection string, redirect to it
                InplaceBuff<4096> location{url_args.extract_query()};
                return redirect(location.size == 0 ? "/" : location.to_string());
            }

            add_notification("error", "Invalid username or password");
        }
    }

    page_template("Login");
    // clang-format off
    append("<div class=\"form-container\">"
               "<h1>Log in</h1>"
               "<form method=\"post\">"
                   // Username
                   "<div class=\"field-group\">"
                       "<label>Username</label>"
                       "<input type=\"text\" name=\"username\" "
                              "value=\"", html_escape(username), "\" "
                              "size=\"24\" "
                              "maxlength=\"", decltype(User::username)::max_len, "\" required>"
                   "</div>"
                   // Password
                   "<div class=\"field-group\">"
                       "<label>Password</label>"
                       "<input type=\"password\" name=\"password\" size=\"24\">"
                   "</div>"
                   // Remember
                   "<div class=\"field-group\">"
                       "<label>Remember me for a month</label>"
                       "<input type=\"checkbox\" name=\"persistent-login\"",
                               (remember ? " checked" : ""), ">"
                   "</div>"
                   "<input class=\"btn blue\" type=\"submit\" value=\"Log in\">"
               "</form>"
           "</div>");
    // clang-format on
}

void Sim::logout() {
    STACK_UNWINDING_MARK;

    session_destroy();
    redirect("/login");
}

void Sim::sign_up() {
    STACK_UNWINDING_MARK;

    if (session_is_open) {
        return redirect("/");
    }

    InplaceBuff<4096> pass1;
    InplaceBuff<4096> pass2;
    decltype(User::username) username;
    decltype(User::first_name) first_name;
    decltype(User::last_name) last_name;
    decltype(User::email) email;

    if (request.method == server::HttpRequest::POST) {
        // Validate all fields
        form_validate_not_blank(
            username, "username", "Username", is_username,
            "Username can only consist of characters [a-zA-Z0-9_-]",
            decltype(User::username)::max_len);
        form_validate_not_blank(
            first_name, "first_name", "First Name", decltype(User::first_name)::max_len);
        form_validate_not_blank(
            last_name, "last_name", "Last Name", decltype(User::first_name)::max_len);
        form_validate_not_blank(email, "email", "Email", decltype(User::email)::max_len);

        if (form_validate(pass1, "password1", "Password") &&
            form_validate(pass2, "password2", "Password (repeat)") && pass1 != pass2)
        {
            add_notification("error", "Passwords do not match");
        }

        // If all fields are ok
        if (not notifications.size) {
            STACK_UNWINDING_MARK;

            static_assert(decltype(User::salt)::max_len % 2 == 0);
            array<char, (decltype(User::salt)::max_len >> 1)> salt_bin{};
            fill_randomly(salt_bin.data(), salt_bin.size());
            auto salt = to_hex({salt_bin.data(), salt_bin.size()});

            auto stmt = mysql.prepare("INSERT IGNORE `users` (username,"
                                      " first_name, last_name, email, salt,"
                                      " password) "
                                      "VALUES(?, ?, ?, ?, ?, ?)");

            stmt.bind_and_execute(
                username, first_name, last_name, email, salt,
                sha3_512(intentional_unsafe_string_view(concat(salt, pass1))));

            // User account successfully created
            if (stmt.affected_rows() == 1) {
                auto new_uid = to_string(stmt.insert_id());

                session_create_and_open(new_uid, true);
                stdlog("New user: ", new_uid, " -> `", username, '`');

                return redirect("/");
            }

            add_notification("error", "Username taken");
        }
    }

    page_template("Sign up");
    // clang-format off
    append("<div class=\"form-container\">"
               "<h1>Sign up</h1>"
               "<form method=\"post\">"
                   // Username
                   "<div class=\"field-group\">"
                       "<label>Username</label>"
                       "<input type=\"text\" name=\"username\" "
                              "value=\"", html_escape(username), "\" "
                              "size=\"24\" "
                              "maxlength=\"", decltype(User::username)::max_len, "\" required>"
                   "</div>"
                   // First Name
                   "<div class=\"field-group\">"
                       "<label>First name</label>"
                       "<input type=\"text\" name=\"first_name\" "
                              "value=\"", html_escape(first_name), "\" "
                              "size=\"24\" "
                              "maxlength=\"", decltype(User::first_name)::max_len, "\" "
                              "required>"
                   "</div>"
                   // Last name
                   "<div class=\"field-group\">"
                       "<label>Last name</label>"
                       "<input type=\"text\" name=\"last_name\" "
                              "value=\"", html_escape(last_name), "\" "
                              "size=\"24\" "
                              "maxlength=\"", decltype(User::last_name)::max_len, "\" "
                              "required>"
                   "</div>"
                   // Email
                   "<div class=\"field-group\">"
                       "<label>Email</label>"
                       "<input type=\"email\" name=\"email\" "
                              "value=\"", html_escape(email), "\" size=\"24\" "
                              "maxlength=\"", decltype(User::email)::max_len, "\" required>"
                   "</div>"
                   // Password
                   "<div class=\"field-group\">"
                       "<label>Password</label>"
                       "<input type=\"password\" name=\"password1\" "
                              "size=\"24\">"
                   "</div>"
                   // Password (repeat)
                   "<div class=\"field-group\">"
                       "<label>Password (repeat)</label>"
                       "<input type=\"password\" name=\"password2\" "
                              "size=\"24\">"
                   "</div>"
                   "<input class=\"btn blue\" type=\"submit\" "
                          "value=\"Sign up\">"
               "</form>"
           "</div>");
    // clang-format on
}

void Sim::users_handle() {
    STACK_UNWINDING_MARK;

    if (not session_is_open) {
        return redirect(concat_tostr("/login?", request.target));
    }

    StringView next_arg = url_args.extract_next_arg();
    if (next_arg == "add") { // Add user
        page_template("Add user");
        append("<script>add_user(false);</script>");

    } else if (auto uid = str2num<decltype(users_uid)>(next_arg)) { // View user
        users_uid = *uid;
        return users_user();

    } else if (next_arg.empty()) { // List users
        page_template("Users", "body{padding-left:20px}");
        append("<script>user_chooser(false, window.location.hash);</script>");

    } else {
        return error404();
    }
}

void Sim::users_user() {
    STACK_UNWINDING_MARK;

    StringView next_arg = url_args.extract_next_arg();
    if (next_arg.empty()) {
        page_template(
            intentional_unsafe_string_view(concat("User ", users_uid)),
            "body{padding-left:20px}");
        append("<script>view_user(false, ", users_uid, ", window.location.hash);</script>");

    } else if (next_arg == "edit") {
        page_template(intentional_unsafe_string_view(concat("Edit user ", users_uid)));
        append("<script>edit_user(false, ", users_uid, ");</script>");

    } else if (next_arg == "delete") {
        page_template(intentional_unsafe_string_view(concat("Delete user ", users_uid)));
        append("<script>delete_user(false, ", users_uid, ");</script>");

    } else if (next_arg == "merge") {
        page_template(intentional_unsafe_string_view(concat("Merge user ", users_uid)));
        append("<script>merge_user(false, ", users_uid, ");</script>");

    } else if (next_arg == "change-password") {
        page_template(
            intentional_unsafe_string_view(concat("Change password of the user ", users_uid)));
        append("<script>change_user_password(false, ", users_uid, ");</script>");

    } else {
        return error404();
    }
}
