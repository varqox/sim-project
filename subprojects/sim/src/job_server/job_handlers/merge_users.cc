#include "merge_users.hh"
#include "sim/jobs/job.hh"
#include "sim/sql/sql.hh"
#include "sim/submissions/submission.hh"

#include <cstdint>
#include <deque>
#include <sim/contest_users/old_contest_user.hh>
#include <sim/mysql/mysql.hh>
#include <sim/submissions/update_final.hh>
#include <sim/users/user.hh>
#include <simlib/utilities.hh>

using sim::jobs::Job;
using sim::sql::DeleteFrom;
using sim::sql::InsertIgnoreInto;
using sim::sql::Select;
using sim::sql::Update;
using sim::users::User;

namespace job_server::job_handlers {

void MergeUsers::run(sim::mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;

    auto transaction = mysql.start_repeatable_read_transaction();

    decltype(Job::aux_id) donor_user_id;
    decltype(Job::aux_id_2) target_user_id;
    {
        auto stmt = mysql.execute(Select("aux_id, aux_id_2").from("jobs").where("id=?", job_id_));
        stmt.res_bind(donor_user_id, target_user_id);
        throw_assert(stmt.next());
    }
    // Assure that both users exist
    decltype(User::type) donor_user_type;
    decltype(User::type) target_user_type;
    {
        auto stmt =
            mysql.execute(Select("username, type").from("users").where("id=?", donor_user_id));
        decltype(User::username) donor_username;
        stmt.res_bind(donor_username, donor_user_type);
        if (!stmt.next()) {
            return set_failure("User to delete does not exist");
        }

        // Logging
        job_log("Merged user's username: ", donor_username);
        job_log("Merged user's type: ", donor_user_type.to_str());
        auto target_user_stmt =
            mysql.execute(Select("type").from("users").where("id=?", target_user_id));
        target_user_stmt.res_bind(target_user_type);
        if (!target_user_stmt.next()) {
            return set_failure("Target user does not exist");
        }
    }

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
            "Needed by the below comparison between user types"
        );
        // Transfer donor's user-type to the target user if the donor has higher permissions
        if (donor_user_type < target_user_type) {
            mysql.execute(
                Update("users").set("type=?", donor_user_type).where("id=?", target_user_id)
            );
        }
    }

    // Transfer sessions
    mysql.execute(
        Update("sessions").set("user_id=?", target_user_id).where("user_id=?", donor_user_id)
    );

    // Transfer problems
    mysql.execute(
        Update("problems").set("owner_id=?", target_user_id).where("owner_id=?", donor_user_id)
    );

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
        // Transfer donor's contest permissions to the target user if the donor has higher
        // permissions (where both users are contest users)
        mysql.execute(
            Update("contest_users tcu")
                .inner_join("contest_users dcu")
                .on("dcu.contest_id=tcu.contest_id AND dcu.user_id=? AND dcu.mode>tcu.mode",
                    donor_user_id)
                .set("tcu.mode=dcu.mode")
                .where("tcu.user_id=?", target_user_id)
        );
        // Transfer donor's contest permissions to target user where target user is not a contest
        // user
        mysql.execute(InsertIgnoreInto("contest_users (user_id, contest_id, mode)")
                          .select("?, contest_id, mode", target_user_id)
                          .from("contest_users")
                          .where("user_id=?", donor_user_id));
        // Donor contest users' will be deleted upon user deletion thanks to foreign key constraint.
    }

    // Transfer contest_files
    mysql.execute(
        Update("contest_files").set("creator=?", target_user_id).where("creator=?", donor_user_id)
    );

    // Collect update finals
    struct FinalToUpdate {
        decltype(sim::submissions::Submission::problem_id) problem_id = 0;
        decltype(sim::submissions::Submission::contest_problem_id) contest_problem_id;
    };

    std::vector<FinalToUpdate> finals_to_update;
    {
        auto stmt = mysql.execute(Select("DISTINCT problem_id, contest_problem_id")
                                      .from("submissions")
                                      .where("user_id=?", donor_user_id));
        FinalToUpdate ftu;
        stmt.res_bind(ftu.problem_id, ftu.contest_problem_id);
        while (stmt.next()) {
            finals_to_update.emplace_back(ftu);
        }
    }

    // Transfer submissions
    mysql.execute(
        Update("submissions").set("user_id=?", target_user_id).where("user_id=?", donor_user_id)
    );

    // Update finals (both contest and problem finals are being taken care of)
    for (const auto& ftu : finals_to_update) {
        sim::submissions::update_final_lock(mysql, target_user_id, ftu.problem_id);
        sim::submissions::update_final(
            mysql, target_user_id, ftu.problem_id, ftu.contest_problem_id, false
        );
    }

    // Transfer jobs
    mysql.execute(Update("jobs").set("creator=?", target_user_id).where("creator=?", donor_user_id)
    );

    // Finally, delete the donor user
    mysql.execute(DeleteFrom("users").where("id=?", donor_user_id));

    job_done(mysql);
    transaction.commit();
}

} // namespace job_server::job_handlers
