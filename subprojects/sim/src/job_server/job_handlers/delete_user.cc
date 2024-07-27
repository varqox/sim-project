#include "common.hh"
#include "delete_user.hh"

#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/sql.hh>
#include <sim/users/user.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/time.hh>

using sim::jobs::Job;
using sim::sql::DeleteFrom;
using sim::sql::InsertInto;
using sim::users::User;

namespace job_server::job_handlers {

void delete_user(
    sim::mysql::Connection& mysql,
    Logger& logger,
    decltype(Job::id) job_id,
    decltype(User::id) user_id
) {
    STACK_UNWINDING_MARK;

    auto transaction = mysql.start_repeatable_read_transaction();

    // Log some info about the deleted user
    {
        auto stmt =
            mysql.execute(sim::sql::Select("username, type").from("users").where("id=?", user_id));
        decltype(User::username) username;
        decltype(User::type) user_type;
        stmt.res_bind(username, user_type);
        if (!stmt.next()) {
            logger("The user with id: ", user_id, " does not exist");
            mark_job_as_cancelled(mysql, logger, job_id);
            transaction.commit();
            return;
        }

        logger("username: ", username);
        logger("type: ", user_type.to_str());
    }

    // Add jobs to delete submission files
    mysql.execute(InsertInto("jobs (created_at, creator, type, priority, status, aux_id)")
                      .select(
                          "?, NULL, ?, ?, ?, file_id",
                          utc_mysql_datetime(),
                          Job::Type::DELETE_INTERNAL_FILE,
                          default_priority(Job::Type::DELETE_INTERNAL_FILE),
                          Job::Status::PENDING
                      )
                      .from("submissions")
                      .where("user_id=?", user_id));

    // Delete user (all necessary actions will take place thanks to foreign key constraints)
    mysql.execute(DeleteFrom("users").where("id=?", user_id));

    mark_job_as_done(mysql, logger, job_id);
    transaction.commit();
}

} // namespace job_server::job_handlers
