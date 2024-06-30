#include "common.hh"
#include "delete_internal_file.hh"

#include <sim/internal_files/internal_file.hh>
#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/sql.hh>
#include <simlib/macros/stack_unwinding.hh>

using sim::internal_files::InternalFile;
using sim::jobs::Job;
using sim::sql::DeleteFrom;

namespace job_server::job_handlers {

void delete_internal_file(
    sim::mysql::Connection& mysql,
    Logger& logger,
    decltype(Job::id) job_id,
    decltype(InternalFile::id) internal_file_id
) {
    STACK_UNWINDING_MARK;

    auto transaction = mysql.start_repeatable_read_transaction();

    logger("Internal file id: ", internal_file_id);
    (void)unlink(sim::internal_files::path_of(internal_file_id).c_str());

    mysql.execute(DeleteFrom("internal_files").where("id=?", internal_file_id));

    mark_job_as_done(mysql, logger, job_id);
    transaction.commit();
}

} // namespace job_server::job_handlers
