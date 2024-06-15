#include "delete_internal_file.hh"

#include <sim/internal_files/old_internal_file.hh>
#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/sql.hh>
#include <simlib/file_manip.hh>

namespace job_server::job_handlers {

void DeleteInternalFile::run(sim::mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;

    auto stmt = mysql.execute(sim::sql::Select("aux_id").from("jobs").where("id=?", job_id_));
    decltype(sim::jobs::Job::aux_id)::value_type internal_file_id;
    stmt.res_bind(internal_file_id);
    throw_assert(stmt.next());

    job_log("Internal file ID: ", internal_file_id);
    (void)unlink(sim::internal_files::old_path_of(internal_file_id));

    auto transaction = mysql.start_repeatable_read_transaction();
    // The internal_file may already be deleted
    mysql.execute(sim::sql::DeleteFrom("internal_files").where("id=?", internal_file_id));

    job_done(mysql);

    transaction.commit();
}

} // namespace job_server::job_handlers
