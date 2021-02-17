#include "sim/constants.hh"
#include "sim/jobs.hh"
#include "sim/user.hh"
#include "sim/utilities.hh"
#include "simlib/random.hh"
#include "simlib/sha.hh"
#include "simlib/utilities.hh"
#include "src/web_interface/sim.hh"

#include <cstdint>
#include <limits>
#include <type_traits>

using sim::User;
using std::array;
using std::string;

void Sim::api_users() {
    STACK_UNWINDING_MARK;
    using PERM = UserPermissions;

    if (not session_is_open) {
        return api_error403();
    }

    // We may read data several times (permission checking), so transaction is
    // used to ensure data consistency
    auto transaction = mysql.start_transaction();

    InplaceBuff<256> query;
    query.append("SELECT id, username, first_name, last_name, email, type "
                 "FROM users");

    enum ColumnIdx { UID, USERNAME, FNAME, LNAME, EMAIL, UTYPE };

    auto query_append = [&, where_was_added = false](auto&&... args) mutable {
        if (where_was_added) {
            query.append(" AND ", std::forward<decltype(args)>(args)...);
        } else {
            query.append(" WHERE ", std::forward<decltype(args)>(args)...);
            where_was_added = true;
        }
    };

    switch (session_user_type) {
    case User::Type::ADMIN: break;
    case User::Type::TEACHER:
    case User::Type::NORMAL: query_append("id=", session_user_id); break;
    }

    // Process restrictions
    auto rows_limit = API_FIRST_QUERY_ROWS_LIMIT;
    StringView next_arg = url_args.extract_next_arg();
    for (uint mask = 0; !next_arg.empty(); next_arg = url_args.extract_next_arg()) {
        constexpr uint ID_COND = 1;
        constexpr uint UTYPE_COND = 2;

        auto arg = decode_uri(next_arg);
        char cond = arg[0];
        StringView arg_id = StringView(arg).substr(1);

        if (cond == 't' and ~mask & UTYPE_COND) { // User type
            if (arg_id == "A") {
                query_append("type=", EnumVal(User::Type::ADMIN).int_val());
            } else if (arg_id == "T") {
                query_append("type=", EnumVal(User::Type::TEACHER).int_val());
            } else if (arg_id == "N") {
                query_append("type=", EnumVal(User::Type::NORMAL).int_val());
            } else {
                return api_error400(
                    intentional_unsafe_string_view(concat("Invalid user type: ", arg_id)));
            }

            mask |= UTYPE_COND;

            // NOLINTNEXTLINE(bugprone-branch-clone)
        } else if (not is_digit(arg_id)) {
            return api_error400();

        } else if (is_one_of(cond, '<', '>') and ~mask & ID_COND) {
            rows_limit = API_OTHER_QUERY_ROWS_LIMIT;
            query_append("id", arg);
            mask |= ID_COND;

        } else if (cond == '=' and ~mask & ID_COND) {
            query_append("id", arg);
            mask |= ID_COND;

        } else {
            return api_error400();
        }
    }

    query.append(" ORDER BY id LIMIT ", rows_limit);
    auto res = mysql.query(query);

    // clang-format off
	append("[\n{\"columns\":["
	           "\"id\","
	           "\"username\","
	           "\"first_name\","
	           "\"last_name\","
	           "\"email\","
	           "\"type\","
	           "\"actions\""
	       "]}");
    // clang-format on

    while (res.next()) {
        append(
            ",\n[", res[UID], ",\"", res[USERNAME], "\",", json_stringify(res[FNAME]), ',',
            json_stringify(res[LNAME]), ',', json_stringify(res[EMAIL]), ',');

        EnumVal<User::Type> utype{
            WONT_THROW(str2num<std::underlying_type_t<User::Type>>(res[UTYPE]).value())};
        switch (utype) {
        case User::Type::ADMIN: append("\"Admin\","); break;
        case User::Type::TEACHER: append("\"Teacher\","); break;
        case User::Type::NORMAL: append("\"Normal\","); break;
        }

        // Allowed actions
        append('"');

        auto perms = users_get_permissions(res[UID], utype);
        if (uint(perms & PERM::VIEW)) {
            append('v');
        }
        if (uint(perms & PERM::EDIT)) {
            append('E');
        }
        if (uint(perms & PERM::CHANGE_PASS)) {
            append('p');
        }
        if (uint(perms & PERM::ADMIN_CHANGE_PASS)) {
            append('P');
        }
        if (uint(perms & PERM::DELETE)) {
            append('D');
        }
        if (uint(perms & PERM::MERGE)) {
            append('M');
        }
        if (uint(perms & PERM::MAKE_ADMIN)) {
            append('A');
        }
        if (uint(perms & PERM::MAKE_TEACHER)) {
            append('T');
        }
        if (uint(perms & PERM::MAKE_NORMAL)) {
            append('N');
        }

        append("\"]");
    }

    append("\n]");
}

void Sim::api_user() {
    STACK_UNWINDING_MARK;

    if (not session_is_open) {
        return api_error403();
    }

    StringView next_arg = url_args.extract_next_arg();
    if (next_arg == "add") {
        users_perms = users_get_overall_permissions();
        return api_user_add();
    }
    if (not is_digit(next_arg) or request.method != server::HttpRequest::POST) {
        return api_error400();
    }

    users_uid = next_arg;
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
    StringView pass = request.form_fields.get_or("pass", "");
    StringView pass1 = request.form_fields.get_or("pass1", "");

    if (pass != pass1) {
        return api_error403("Passwords do not match");
    }

    StringView username;
    StringView utype_str;
    StringView fname;
    StringView lname;
    StringView email;

    form_validate_not_blank(
        username, "username", "Username", is_username,
        "Username can only consist of characters [a-zA-Z0-9_-]",
        decltype(User::username)::max_len);

    // Validate user type
    utype_str = request.form_fields.get_or("type", "");
    User::Type utype = User::Type::NORMAL; // Silence GCC warning
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
    static_assert(decltype(User::salt)::max_len % 2 == 0);
    array<char, (decltype(User::salt)::max_len >> 1)> salt_bin{};
    fill_randomly(salt_bin.data(), salt_bin.size());
    auto salt = to_hex({salt_bin.data(), salt_bin.size()});

    auto stmt = mysql.prepare("INSERT IGNORE users (username, type,"
                              " first_name, last_name, email, salt, password) "
                              "VALUES(?, ?, ?, ?, ?, ?, ?)");

    stmt.bind_and_execute(
        username, uint(utype), fname, lname, email, salt,
        sha3_512(intentional_unsafe_string_view(concat(salt, pass))));

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
    StringView new_utype_str;
    StringView fname;
    StringView lname;
    StringView email;
    // Validate fields
    form_validate_not_blank(
        username, "username", "Username", is_username,
        "Username can only consist of characters [a-zA-Z0-9_-]",
        decltype(User::username)::max_len);

    // Validate user type
    new_utype_str = request.form_fields.get_or("type", "");
    User::Type new_utype = User::Type::NORMAL;
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
        mysql.prepare("DELETE FROM session WHERE user_id=? AND id!=?")
            .bind_and_execute(users_uid, session_id);
    }

    mysql
        .prepare("UPDATE IGNORE users "
                 "SET username=?, first_name=?, last_name=?, email=?, type=? "
                 "WHERE id=?")
        .bind_and_execute(username, fname, lname, email, uint(new_utype), users_uid);

    transaction.commit();
}

void Sim::api_user_change_password() {
    STACK_UNWINDING_MARK;
    using PERM = UserPermissions;

    if (uint(~users_perms & PERM::CHANGE_PASS) and
        uint(~users_perms & PERM::ADMIN_CHANGE_PASS)) {
        return api_error403();
    }

    StringView new_pass = request.form_fields.get_or("new_pass", "");
    StringView new_pass1 = request.form_fields.get_or("new_pass1", "");

    if (new_pass != new_pass1) {
        return api_error400("Passwords do not match");
    }

    if (uint(~users_perms & PERM::ADMIN_CHANGE_PASS) and
        not check_submitted_password("old_pass")) {
        return api_error403("Invalid old password");
    }

    // Commit password change
    static_assert(decltype(User::salt)::max_len % 2 == 0);
    array<char, (decltype(User::salt)::max_len >> 1)> salt_bin{};
    fill_randomly(salt_bin.data(), salt_bin.size());
    InplaceBuff<decltype(User::salt)::max_len> salt(
        to_hex({salt_bin.data(), salt_bin.size()}));

    auto transaction = mysql.start_transaction();

    mysql.prepare("UPDATE users SET salt=?, password=? WHERE id=?")
        .bind_and_execute(
            salt, sha3_512(intentional_unsafe_string_view(concat(salt, new_pass))), users_uid);

    // Remove other sessions (for security reasons)
    mysql.prepare("DELETE FROM session WHERE user_id=? AND id!=?")
        .bind_and_execute(users_uid, session_id);

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
        session_user_id, EnumVal(JobStatus::PENDING), priority(JobType::DELETE_USER),
        EnumVal(JobType::DELETE_USER), mysql_date(), users_uid);

    jobs::notify_job_server();
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
            std::numeric_limits<decltype(jobs::MergeUsersInfo::target_user_id)>::max()>);

    if (notifications.size) {
        return api_error400(notifications);
    }

    if (donor_user_id == target_user_id) {
        return api_error400("You cannot merge user with themselves");
    }

    auto target_user_perms = users_get_permissions(target_user_id);
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
        session_user_id, EnumVal(JobStatus::PENDING), priority(JobType::MERGE_USERS),
        EnumVal(JobType::MERGE_USERS), mysql_date(), donor_user_id,
        jobs::MergeUsersInfo(WONT_THROW(str2num<uintmax_t>(target_user_id).value())).dump());

    jobs::notify_job_server();
    append(stmt.insert_id());
}
