#include "delete_internal_file.hh"
#include "../main.hh"

#include <sim/constants.hh>

namespace job_handlers {

void DeleteInternalFile::run() {
	STACK_UNWINDING_MARK;

	job_log("Internal file ID: ", internal_file_id_);
	(void)unlink(internal_file_path(internal_file_id_));

	auto transaction = mysql.start_transaction();
	// The internal_file may already be deleted
	mysql.prepare("DELETE FROM internal_files WHERE id=?")
	   .bind_and_execute(internal_file_id_);

	job_done();

	transaction.commit();
}

} // namespace job_handlers
