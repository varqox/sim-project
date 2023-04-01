#include "../main.hh"
#include "delete_internal_file.hh"

#include <sim/internal_files/internal_file.hh>

namespace job_server::job_handlers {

void DeleteInternalFile::run() {
    STACK_UNWINDING_MARK;

    job_log("Internal file ID: ", internal_file_id_);
    (void)unlink(sim::internal_files::path_of(internal_file_id_));

    auto transaction = mysql.start_transaction();
    // The internal_file may already be deleted
    mysql.prepare("DELETE FROM internal_files WHERE id=?").bind_and_execute(internal_file_id_);

    job_done();

    transaction.commit();
}

} // namespace job_server::job_handlers
