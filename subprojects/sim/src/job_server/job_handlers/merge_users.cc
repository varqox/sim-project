#include "common.hh"
#include "merge_users.hh"

#include <sim/contest_users/contest_user.hh>
#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/sql.hh>
#include <sim/submissions/submission.hh>
#include <sim/submissions/update_final.hh>
#include <sim/users/user.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/utilities.hh>

using sim::contest_users::ContestUser;
using sim::jobs::Job;
using sim::sql::DeleteFrom;
using sim::sql::InsertIgnoreInto;
using sim::sql::Select;
using sim::sql::Update;
using sim::users::User;

namespace job_server::job_handlers {

void merge_users(
    sim::mysql::Connection& mysql,
    Logger& logger,
    decltype(Job::id) job_id,
    decltype(User::id) donor_user_id,
    decltype(User::id) target_user_id
) {
    STACK_UNWINDING_MARK;
    auto transaction = mysql.start_repeatable_read_transaction();

    // Assure that both users exist
    decltype(User::type) donor_user_type;
    decltype(User::type) target_user_type;
    {
        auto stmt =
            mysql.execute(Select("username, type").from("users").where("id=?", donor_user_id));
        decltype(User::username) donor_username;
        stmt.res_bind(donor_username, donor_user_type);
        if (!stmt.next()) {
            logger("User to delete does not exist");
            mark_job_as_cancelled(mysql, logger, job_id);
            transaction.commit();
            return;
        }

        // Logging
        logger("Donor user's username: ", donor_username);
        logger("Donor user's type: ", donor_user_type.to_str());
        auto target_user_stmt =
            mysql.execute(Select("type").from("users").where("id=?", target_user_id));
        target_user_stmt.res_bind(target_user_type);
        if (!target_user_stmt.next()) {
            logger("Target user does not exist");
            mark_job_as_cancelled(mysql, logger, job_id);
            transaction.commit();
            return;
        }
    }

    // Transfer user type if gives more permissions
    {
        (void)[](User::Type x) {
            // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
            switch (x) { // If compiler warns here, update the below static_assert
            case User::Type::ADMIN:
            case User::Type::TEACHER:
            case User::Type::NORMAL:
                (void)"switch is used only to make compiler warn if the User::Type gets updated, "
                      "so that you notice that the below static_assert needs updating";
            }
            // clang-format off
        };
        // clang-format on
        static_assert(
            is_sorted(std::array{
                EnumVal(User::Type::ADMIN),
                EnumVal(User::Type::TEACHER),
                EnumVal(User::Type::NORMAL),
            }),
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
        (void)[](ContestUser::Mode x) {
            // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
            switch (x) { // If compiler warns here, update the below static_assert
            case ContestUser::Mode::CONTESTANT:
            case ContestUser::Mode::MODERATOR:
            case ContestUser::Mode::OWNER:
                (void)"switch is used only to make compiler warn if the ContestUser::Mode gets "
                      "updated, so that you notice that the below static_assert needs updating";
            }
            // clang-format off
        };
        // clang-format on
        static_assert(
            is_sorted(std::array{
                EnumVal(ContestUser::Mode::CONTESTANT),
                EnumVal(ContestUser::Mode::MODERATOR),
                EnumVal(ContestUser::Mode::OWNER)
            }),
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
        sim::submissions::update_final(
            mysql, target_user_id, ftu.problem_id, ftu.contest_problem_id
        );
    }

    // Transfer jobs
    mysql.execute(Update("jobs").set("creator=?", target_user_id).where("creator=?", donor_user_id)
    );

    // Finally, delete the donor user
    mysql.execute(DeleteFrom("users").where("id=?", donor_user_id));

    mark_job_as_done(mysql, logger, job_id);
    transaction.commit();
}

} // namespace job_server::job_handlers
