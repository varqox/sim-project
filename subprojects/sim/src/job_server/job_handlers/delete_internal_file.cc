#include "delete_internal_file.hh"

#include <sim/internal_files/old_internal_file.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/sql.hh>
#include <simlib/file_manip.hh>

namespace job_server::job_handlers {

void DeleteInternalFile::run(sim::mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;

    job_log("Internal file ID: ", internal_file_id_);
    (void)unlink(sim::internal_files::path_of(internal_file_id_));

    auto transaction = mysql.start_repeatable_read_transaction();
    // The internal_file may already be deleted
    mysql.execute(sim::sql::DeleteFrom("internal_files").where("id=?", internal_file_id_));

    job_done(mysql);

    transaction.commit();
}

} // namespace job_server::job_handlers
