#include "../progress_printer.hh"
#include "../sim_instance.hh"
#include "modify_main_table_ids.hh"
#include "upadte_jobs_using_table_ids.hh"
#include "users_merger.hh"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <sim/jobs/job.hh>
#include <sim/merging/merge_ids.hh>
#include <sim/sql/fields/datetime.hh>
#include <sim/sql/sql.hh>
#include <sim/users/user.hh>
#include <simlib/logger.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <unordered_map>

using sim::sql::InsertInto;
using sim::sql::Select;
using sim::sql::Update;
using sim::users::User;

namespace mergers {

std::optional<sim::sql::fields::Datetime> UsersIdIterator::created_at_upper_bound_of_id(uint64_t id
) {
    STACK_UNWINDING_MARK;
    using sim::jobs::Job;

    auto stmt = mysql.execute(
        Select("created_at")
            .from("jobs")
            .where(
                "creator=? OR (type=? AND aux_id=?) OR (type=? AND ? IN (aux_id, aux_id_2))",
                id,
                Job::Type::DELETE_USER,
                id,
                Job::Type::MERGE_USERS,
                id
            )
            .order_by("id")
            .limit("1")
    );
    sim::sql::fields::Datetime created_at;
    stmt.res_bind(created_at);
    if (!stmt.next()) {
        return std::nullopt;
    }
    return created_at;
}

UsersMerger::UsersMerger(const SimInstance& main_sim, const SimInstance& other_sim)
: main_sim{main_sim}
, other_sim{other_sim}
, main_ids_iter{main_sim.mysql} {
    STACK_UNWINDING_MARK;

    auto other_ids_iter = UsersIdIterator{other_sim.mysql};

    merged_ids = sim::merging::merge_ids(main_ids_iter, other_ids_iter);
    // The problem is that usernames cannot conflict, so we have to merge such users now. So
    // first we need to identify the other ids that will disappear.
    decltype(User::id) other_id;
    decltype(User::username) other_username;
    auto other_stmt = other_sim.mysql.execute(Select("id, username").from("users"));
    other_stmt.res_bind(other_id, other_username);
    while (other_stmt.next()) {
        auto stmt =
            main_sim.mysql.execute(Select("id").from("users").where("username=?", other_username));
        decltype(User::id) main_id;
        stmt.res_bind(main_id);
        if (stmt.next()) {
            stdlog(
                "found username to merge ",
                other_username,
                ": other ",
                other_id,
                " -> main ",
                main_id
            );
            other_id_to_main_id_with_the_same_username[other_id] = main_id;
            merged_ids_of_merged_other_users.emplace_back(merged_ids.other_id_to_new_id(other_id));
        }
    }

    std::sort(merged_ids_of_merged_other_users.begin(), merged_ids_of_merged_other_users.end());
}

decltype(User::id) UsersMerger::main_id_to_new_id(decltype(User::id) main_id) const noexcept {
    return shifted_new_id(merged_ids.current_id_to_new_id(main_id));
}

decltype(User::id) UsersMerger::other_id_to_new_id(decltype(User::id) other_id) const noexcept {
    auto it = other_id_to_main_id_with_the_same_username.find(other_id);
    if (it != other_id_to_main_id_with_the_same_username.end()) {
        return main_id_to_new_id(it->second);
    }

    return shifted_new_id(merged_ids.other_id_to_new_id(other_id));
}

[[nodiscard]] decltype(User::id) UsersMerger::shifted_new_id(decltype(User::id) new_id
) const noexcept {
    const size_t shift =
        std::lower_bound(
            merged_ids_of_merged_other_users.begin(), merged_ids_of_merged_other_users.end(), new_id
        ) -
        merged_ids_of_merged_other_users.begin();
    assert(
        (shift == merged_ids_of_merged_other_users.size() ||
         merged_ids_of_merged_other_users[shift] != new_id) &&
        "The id we search for cannot be deleted"
    );
    return new_id - shift;
}

void UsersMerger::merge_and_save() {
    STACK_UNWINDING_MARK;

    modify_main_table_ids(
        main_ids_iter,
        main_sim.mysql,
        "users",
        [this](decltype(User::id) main_id) { return main_id_to_new_id(main_id); }
    );

    update_jobs_using_table_ids(
        "users",
        main_ids_iter.max_id_plus_one(),
        main_ids_iter.min_id(),
        [this](decltype(User::id) main_id) { return main_id_to_new_id(main_id); },
        [this](decltype(User::id) main_id, decltype(User::id) new_id) {
            using sim::jobs::Job;
            STACK_UNWINDING_MARK;

            main_sim.mysql.execute(
                Update("jobs").set("creator=?", new_id).where("creator=?", main_id)
            );
            main_sim.mysql.execute(Update("jobs")
                                       .set("aux_id=?", new_id)
                                       .where(
                                           "type IN (?, ?) AND aux_id=?",
                                           Job::Type::DELETE_USER,
                                           Job::Type::MERGE_USERS,
                                           main_id
                                       ));
            main_sim.mysql.execute(
                Update("jobs")
                    .set("aux_id_2=?", new_id)
                    .where("type=? AND aux_id_2=?", Job::Type::MERGE_USERS, main_id)
            );
        }
    );

    // Save other users
    auto other_users_copying_progress_printer =
        ProgressPrinter<decltype(User::id)>{"progress: copying other user with id:"};
    static_assert(User::COLUMNS_NUM == 9, "Update the statements below");
    auto stmt = other_sim.mysql.execute(Select("id, created_at, type, username, first_name, "
                                               "last_name, email, password_salt, password_hash")
                                            .from("users")
                                            .order_by("id DESC"));
    stmt.do_not_store_result(); // minimize memory usage
    User user;
    stmt.res_bind(
        user.id,
        user.created_at,
        user.type,
        user.username,
        user.first_name,
        user.last_name,
        user.email,
        user.password_salt,
        user.password_hash
    );
    while (stmt.next()) {
        other_users_copying_progress_printer.note_id(user.id);
        if (other_id_to_main_id_with_the_same_username.count(user.id) == 1) {
            continue;
        }
        main_sim.mysql.execute(InsertInto("users (id, created_at, type, username, first_name, "
                                          "last_name, email, password_salt, password_hash)")
                                   .values(
                                       "?, ?, ?, ?, ?, ?, ?, ?, ?",
                                       other_id_to_new_id(user.id),
                                       user.created_at,
                                       user.type,
                                       user.username,
                                       user.first_name,
                                       user.last_name,
                                       user.email,
                                       user.password_salt,
                                       user.password_hash
                                   ));
    }
}

} // namespace mergers
