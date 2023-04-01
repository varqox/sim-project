#include "../main.hh"
#include "delete_user.hh"

#include <sim/jobs/job.hh>
#include <sim/users/user.hh>

using sim::jobs::Job;
using sim::users::User;

namespace job_server::job_handlers {

void DeleteUser::run() {
    STACK_UNWINDING_MARK;

    auto transaction = mysql.start_transaction();

    // Log some info about the deleted user
    {
        auto stmt = mysql.prepare("SELECT username, type FROM users WHERE id=?");
        stmt.bind_and_execute(user_id_);
        InplaceBuff<32> username;
        decltype(User::type) user_type;
        stmt.res_bind_all(username, user_type);
        if (not stmt.next()) {
            return set_failure("User with id: ", user_id_, " does not exist");
        }

        job_log("username: ", username);
        job_log("type: ", user_type.to_enum().to_str());
    }

    // Add jobs to delete submission files
    mysql
        .prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
                 " added, aux_id, info, data) "
                 "SELECT file_id, NULL, ?, ?, ?, ?, NULL, '', ''"
                 " FROM submissions WHERE owner=?")
        .bind_and_execute(
            EnumVal(Job::Type::DELETE_FILE),
            default_priority(Job::Type::DELETE_FILE),
            EnumVal(Job::Status::PENDING),
            mysql_date(),
            user_id_
        );

    // Delete user (all necessary actions will take place thanks to foreign key
    // constrains)
    mysql.prepare("DELETE FROM users WHERE id=?").bind_and_execute(user_id_);

    job_done();

    transaction.commit();
}

} // namespace job_server::job_handlers
