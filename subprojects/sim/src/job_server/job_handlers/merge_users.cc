#include "merge_users.hh"

#include <deque>
#include <sim/contest_users/old_contest_user.hh>
#include <sim/mysql/mysql.hh>
#include <sim/submissions/update_final.hh>
#include <sim/users/user.hh>
#include <simlib/utilities.hh>

using sim::users::User;

namespace job_server::job_handlers {

void MergeUsers::run(sim::mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;

    for (;;) {
        try {
            run_impl(mysql);
            break;
        } catch (const std::exception& e) {
            if (has_prefix(
                    e.what(),
                    "Deadlock found when trying to get lock; "
                    "try restarting transaction"
                ))
            {
                continue;
            }

            throw;
        }
    }
}

void MergeUsers::run_impl(sim::mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;

    auto transaction = mysql.start_repeatable_read_transaction();
    using sim::sql::Select;

    // Assure that both users exist
    decltype(User::type) donor_user_type;
    {
        auto stmt =
            mysql.execute(Select("username, type").from("users").where("id=?", donor_user_id_));
        decltype(User::username) donor_username;
        stmt.res_bind(donor_username, donor_user_type);
        if (not stmt.next()) {
            return set_failure("User to delete does not exist");
        }

        // Logging
        job_log("Merged user's username: ", donor_username);
        job_log("Merged user's type: ", donor_user_type.to_str());

        stmt = mysql.execute(Select("1").from("users").where("id=?", info_.target_user_id));
        if (not stmt.next()) {
            return set_failure("Target user does not exist");
        }
    }

    auto old_mysql = old_mysql::ConnectionView{mysql};
    // Transfer user type if gives more permissions
    {
        using UT = User::Type;
        (void)[](UT x) {
            switch (x) { // If compiler warns here, update the below static_assert
            case UT::ADMIN:
            case UT::TEACHER:
            case UT::NORMAL:
                (void)"switch is used only to make compiler warn if the UT "
                      "gets updated";
            }
            // clang-format off
        };
        // clang-format on
        static_assert(
            is_sorted(std::array{EnumVal(UT::ADMIN), EnumVal(UT::TEACHER), EnumVal(UT::NORMAL)}),
            "Needed by below SQL statement (comparison between types)"
        );
        // Transfer donor's user-type to the target user if the donor has higher
        // permissions
        old_mysql
            .prepare("UPDATE users tu "
                     "JOIN users du ON du.id=? AND du.type<tu.type "
                     "SET tu.type=du.type WHERE tu.id=?")
            .bind_and_execute(donor_user_id_, info_.target_user_id);
    }

    // Transfer sessions
    old_mysql.prepare("UPDATE sessions SET user_id=? WHERE user_id=?")
        .bind_and_execute(info_.target_user_id, donor_user_id_);

    // Transfer problems
    old_mysql.prepare("UPDATE problems SET owner_id=? WHERE owner_id=?")
        .bind_and_execute(info_.target_user_id, donor_user_id_);

    // Transfer contest_users
    {
        using CUM = sim::contest_users::OldContestUser::Mode;
        (void)[](CUM x) {
            switch (x) { // If compiler warns here, update the below static_assert
            case CUM::CONTESTANT:
            case CUM::MODERATOR:
            case CUM::OWNER:
                (void)"switch is used only to make compiler warn if the CUM "
                      "gets updated";
            }
            // clang-format off
        };
        // clang-format on
        static_assert(
            is_sorted(
                std::array{EnumVal(CUM::CONTESTANT), EnumVal(CUM::MODERATOR), EnumVal(CUM::OWNER)}
            ),
            "Needed by below SQL statement (comparison between modes)"
        );
        // Transfer donor's contest permissions to the target user if the donor
        // has higher permissions
        old_mysql
            .prepare("UPDATE contest_users tu "
                     "JOIN contest_users du ON du.user_id=?"
                     " AND du.contest_id=tu.contest_id AND du.mode>tu.mode "
                     "SET tu.mode=du.mode WHERE tu.user_id=?")
            .bind_and_execute(donor_user_id_, info_.target_user_id);
        // Transfer donor's contest permissions that target user does not have
        old_mysql.prepare("UPDATE IGNORE contest_users SET user_id=? WHERE user_id=?")
            .bind_and_execute(info_.target_user_id, donor_user_id_);
    }

    // Transfer contest_files
    old_mysql.prepare("UPDATE contest_files SET creator=? WHERE creator=?")
        .bind_and_execute(info_.target_user_id, donor_user_id_);

    // Collect update finals
    struct FTU {
        uint64_t problem_id{};
        old_mysql::Optional<uint64_t> contest_problem_id;
    };

    std::deque<FTU> finals_to_update;
    {
        auto stmt = old_mysql.prepare("SELECT DISTINCT problem_id, contest_problem_id FROM "
                                      "submissions WHERE owner=?");
        stmt.bind_and_execute(donor_user_id_);
        FTU ftu_elem;
        stmt.res_bind_all(ftu_elem.problem_id, ftu_elem.contest_problem_id);
        while (stmt.next()) {
            finals_to_update.emplace_back(ftu_elem);
        }
    }

    // Transfer submissions
    old_mysql.prepare("UPDATE submissions SET owner=? WHERE owner=?")
        .bind_and_execute(info_.target_user_id, donor_user_id_);

    // Update finals (both contest and problem finals are being taken care of)
    for (const auto& ftu_elem : finals_to_update) {
        sim::submissions::update_final_lock(mysql, info_.target_user_id, ftu_elem.problem_id);
        sim::submissions::update_final(
            mysql, info_.target_user_id, ftu_elem.problem_id, ftu_elem.contest_problem_id, false
        );
    }

    // Transfer jobs
    old_mysql.prepare("UPDATE jobs SET creator=? WHERE creator=?")
        .bind_and_execute(info_.target_user_id, donor_user_id_);

    // Finally, delete the donor user
    old_mysql.prepare("DELETE FROM users WHERE id=?").bind_and_execute(donor_user_id_);

    job_done(mysql);
    transaction.commit();
}

} // namespace job_server::job_handlers
