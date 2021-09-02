#include "sim/is_username.hh"
#include "sim/jobs/utils.hh"
#include "sim/users/user.hh"
#include "simlib/random.hh"
#include "simlib/sha.hh"
#include "simlib/string_transform.hh"
#include "simlib/time.hh"
#include "simlib/utilities.hh"
#include "src/web_server/old/sim.hh"

#include <cstdint>
#include <limits>
#include <type_traits>

using sim::is_username;
using sim::jobs::Job;
using sim::users::User;
using std::array;
using std::string;

namespace web_server::old {

void Sim::api_user() {
    STACK_UNWINDING_MARK;

    if (not session.has_value()) {
        return api_error403();
    }

    StringView next_arg = url_args.extract_next_arg();
    if (next_arg == "add") {
        users_perms = users_get_overall_permissions();
        return api_user_add();
    }
    if (not is_digit(next_arg) or request.method != http::Request::POST) {
        return api_error400();
    }

    auto uid_opt = str2num<decltype(users_uid)>(next_arg);
    if (not uid_opt) {
        return api_error404();
    }
    users_uid = *uid_opt;
    users_perms = users_get_permissions(users_uid);
    if (users_perms == UserPermissions::NONE) {
        return api_error404();
    }

    next_arg = url_args.extract_next_arg();
    if (next_arg == "edit") {
        return api_user_edit();
    }
    if (next_arg == "delete") {
        return api_user_delete();
    }
    if (next_arg == "merge_into_another") {
        return api_user_merge_into_another();
    }
    if (next_arg == "change-password") {
        return api_user_change_password();
    }
    return api_error404();
}

void Sim::api_user_add() {
    STACK_UNWINDING_MARK;
    using PERM = UserPermissions;

    if (uint(~users_perms & PERM::ADD_USER)) {
        return api_error403();
    }

    // Validate fields
    StringView pass = request.form_fields.get("pass").value_or("");
    StringView pass1 = request.form_fields.get("pass1").value_or("");

    if (pass != pass1) {
        return api_error403("Passwords do not match");
    }

    StringView username;
    StringView fname;
    StringView lname;
    StringView email;

    form_validate_not_blank(
        username, "username", "Username", is_username,
        "Username can only consist of characters [a-zA-Z0-9_-]",
        decltype(User::username)::max_len);

    // Validate user type
    auto utype_str = request.form_fields.get("type");
    decltype(User::type) utype = User::Type::NORMAL; // Silence GCC warning
    if (utype_str == "A") {
        utype = User::Type::ADMIN;
        if (uint(~users_perms & PERM::ADD_ADMIN)) {
            add_notification("error", "You have no permissions to make this user an admin");
        }

    } else if (utype_str == "T") {
        utype = User::Type::TEACHER;
        if (uint(~users_perms & PERM::ADD_TEACHER)) {
            add_notification("error", "You have no permissions to make this user a teacher");
        }

    } else if (utype_str == "N") {
        utype = User::Type::NORMAL;
        if (uint(~users_perms & PERM::ADD_NORMAL)) {
            add_notification(
                "error", "You have no permissions to make this user a normal user");
        }

    } else {
        add_notification("error", "Invalid user's type");
    }

    form_validate_not_blank(
        fname, "first_name", "First Name", decltype(User::first_name)::max_len);

    form_validate_not_blank(
        lname, "last_name", "Last Name", decltype(User::last_name)::max_len);

    form_validate_not_blank(email, "email", "Email", decltype(User::email)::max_len);

    if (notifications.size) {
        return api_error400(notifications);
    }

    // All fields are valid
    auto [salt, hash] = sim::users::salt_and_hash_password(pass);
    auto stmt = mysql.prepare("INSERT IGNORE users (username, type,"
                              " first_name, last_name, email, password_salt, password_hash) "
                              "VALUES(?, ?, ?, ?, ?, ?, ?)");
    stmt.bind_and_execute(username, utype, fname, lname, email, salt, hash);

    // User account successfully created
    if (stmt.affected_rows() != 1) {
        return api_error400("Username taken");
    }

    stdlog("New user: ", stmt.insert_id(), " -> `", username, '`');
}

void Sim::api_user_edit() {
    STACK_UNWINDING_MARK;
    using PERM = UserPermissions;

    if (uint(~users_perms & PERM::EDIT)) {
        return api_error403();
    }

    // TODO: this looks very similar to the above validation
    StringView username;
    StringView fname;
    StringView lname;
    StringView email;
    // Validate fields
    form_validate_not_blank(
        username, "username", "Username", is_username,
        "Username can only consist of characters [a-zA-Z0-9_-]",
        decltype(User::username)::max_len);

    // Validate user type
    auto new_utype_str = request.form_fields.get("type");
    decltype(User::type) new_utype = User::Type::NORMAL;
    if (new_utype_str == "A") {
        new_utype = User::Type::ADMIN;
        if (uint(~users_perms & PERM::MAKE_ADMIN)) {
            add_notification("error", "You have no permissions to make this user an admin");
        }
    } else if (new_utype_str == "T") {
        new_utype = User::Type::TEACHER;
        if (uint(~users_perms & PERM::MAKE_TEACHER)) {
            add_notification("error", "You have no permissions to make this user a teacher");
        }
    } else if (new_utype_str == "N") {
        new_utype = User::Type::NORMAL;
        if (uint(~users_perms & PERM::MAKE_NORMAL)) {
            add_notification(
                "error", "You have no permissions to make this user a normal user");
        }
    } else {
        add_notification("error", "Invalid user's type");
    }

    form_validate_not_blank(
        fname, "first_name", "First Name", decltype(User::first_name)::max_len);

    form_validate_not_blank(
        lname, "last_name", "Last Name", decltype(User::last_name)::max_len);

    form_validate_not_blank(email, "email", "Email", decltype(User::email)::max_len);

    if (notifications.size) {
        return api_error400(notifications);
    }

    auto transaction = mysql.start_transaction();

    auto stmt = mysql.prepare("SELECT 1 FROM users WHERE username=? AND id!=?");
    stmt.bind_and_execute(username, users_uid);
    if (stmt.next()) {
        return api_error400("Username is already taken");
    }

    // If username changes, remove other sessions (for security reasons)
    stmt = mysql.prepare("SELECT 1 FROM users WHERE username=? AND id=?");
    stmt.bind_and_execute(username, users_uid);
    if (not stmt.next()) {
        mysql.prepare("DELETE FROM sessions WHERE user_id=? AND id!=?")
            .bind_and_execute(users_uid, session->id);
    }

    mysql
        .prepare("UPDATE IGNORE users "
                 "SET username=?, first_name=?, last_name=?, email=?, type=? "
                 "WHERE id=?")
        .bind_and_execute(username, fname, lname, email, new_utype, users_uid);

    transaction.commit();
}

void Sim::api_user_change_password() {
    STACK_UNWINDING_MARK;
    using PERM = UserPermissions;

    if (uint(~users_perms & PERM::CHANGE_PASS) and
        uint(~users_perms & PERM::ADMIN_CHANGE_PASS)) {
        return api_error403();
    }

    StringView new_pass = request.form_fields.get("new_pass").value_or("");
    StringView new_pass1 = request.form_fields.get("new_pass1").value_or("");

    if (new_pass != new_pass1) {
        return api_error400("Passwords do not match");
    }

    if (uint(~users_perms & PERM::ADMIN_CHANGE_PASS) and
        not check_submitted_password("old_pass")) {
        return api_error403("Invalid old password");
    }

    // Commit password change
    auto [salt, hash] = sim::users::salt_and_hash_password(new_pass);
    auto transaction = mysql.start_transaction();
    mysql.prepare("UPDATE users SET password_salt=?, password_hash=? WHERE id=?")
        .bind_and_execute(salt, hash, users_uid);

    // Remove other sessions (for security reasons)
    mysql.prepare("DELETE FROM sessions WHERE user_id=? AND id!=?")
        .bind_and_execute(users_uid, session->id);

    transaction.commit();
}

void Sim::api_user_delete() {
    STACK_UNWINDING_MARK;

    if (uint(~users_perms & UserPermissions::DELETE)) {
        return api_error403();
    }

    if (not check_submitted_password()) {
        return api_error403("Invalid password");
    }

    // Queue deleting job
    auto stmt = mysql.prepare("INSERT jobs (creator, status, priority, type,"
                              " added, aux_id, info, data)"
                              "VALUES(?, ?, ?, ?, ?, ?, '', '')");
    stmt.bind_and_execute(
        session->user_id, EnumVal(Job::Status::PENDING),
        default_priority(Job::Type::DELETE_USER), EnumVal(Job::Type::DELETE_USER),
        mysql_date(), users_uid);

    sim::jobs::notify_job_server();
    append(stmt.insert_id());
}

void Sim::api_user_merge_into_another() {
    STACK_UNWINDING_MARK;

    auto donor_user_id = users_uid;
    if (uint(~users_perms & UserPermissions::MERGE)) {
        return api_error403();
    }

    InplaceBuff<32> target_user_id;
    form_validate_not_blank(
        target_user_id, "target_user", "Target user ID",
        is_digit_not_greater_than<
            std::numeric_limits<decltype(sim::jobs::MergeUsersInfo::target_user_id)>::max()>);

    if (notifications.size) {
        return api_error400(notifications);
    }

    if (donor_user_id == str2num<decltype(donor_user_id)>(target_user_id)) {
        return api_error400("You cannot merge user with themselves");
    }

    auto target_user_perms = [&] {
        if (auto uid_opt = str2num<decltype(User::id)>(target_user_id)) {
            return users_get_permissions(*uid_opt);
        }
        return UserPermissions::NONE;
    }();
    if (uint(~target_user_perms & UserPermissions::MERGE)) {
        return api_error403("You do not have permission to merge to the target user");
    }

    if (not check_submitted_password()) {
        return api_error403("Invalid password");
    }

    // Queue merging job
    auto stmt = mysql.prepare("INSERT jobs (creator, status, priority, type,"
                              " added, aux_id, info, data)"
                              "VALUES(?, ?, ?, ?, ?, ?, ?, '')");
    stmt.bind_and_execute(
        session->user_id, EnumVal(Job::Status::PENDING),
        default_priority(Job::Type::MERGE_USERS), EnumVal(Job::Type::MERGE_USERS),
        mysql_date(), donor_user_id,
        sim::jobs::MergeUsersInfo(
            WONT_THROW(
                str2num<decltype(sim::jobs::MergeUsersInfo::target_user_id)>(target_user_id)
                    .value()))
            .dump());

    sim::jobs::notify_job_server();
    append(stmt.insert_id());
}

} // namespace web_server::old
