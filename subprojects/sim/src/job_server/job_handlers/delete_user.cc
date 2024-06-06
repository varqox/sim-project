#include "delete_user.hh"

#include <sim/jobs/old_job.hh>
#include <sim/mysql/mysql.hh>
#include <sim/old_mysql/old_mysql.hh>
#include <sim/sql/sql.hh>
#include <sim/users/old_user.hh>
#include <sim/users/user.hh>
#include <simlib/time.hh>

using sim::jobs::OldJob;
using sim::users::User;

namespace job_server::job_handlers {

void DeleteUser::run(sim::mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;

    auto transaction = mysql.start_repeatable_read_transaction();

    // Log some info about the deleted user
    {
        auto stmt =
            mysql.execute(sim::sql::Select("username, type").from("users").where("id=?", user_id_));
        decltype(User::username) username;
        decltype(User::type) user_type;
        stmt.res_bind(username, user_type);
        if (not stmt.next()) {
            return set_failure("User with id: ", user_id_, " does not exist");
        }

        job_log("username: ", username);
        job_log("type: ", user_type.to_str());
    }

    // Add jobs to delete submission files
    auto old_mysql = old_mysql::ConnectionView{mysql};
    old_mysql
        .prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
                 " created_at, aux_id, info, data) "
                 "SELECT file_id, NULL, ?, ?, ?, ?, NULL, '', ''"
                 " FROM submissions WHERE user_id=?")
        .bind_and_execute(
            EnumVal(OldJob::Type::DELETE_FILE),
            default_priority(OldJob::Type::DELETE_FILE),
            EnumVal(OldJob::Status::PENDING),
            utc_mysql_datetime(),
            user_id_
        );

    // Delete user (all necessary actions will take place thanks to foreign key
    // constrains)
    old_mysql.prepare("DELETE FROM users WHERE id=?").bind_and_execute(user_id_);

    job_done(mysql);

    transaction.commit();
}

} // namespace job_server::job_handlers
