#include "delete_internal_file.h"
#include "../main.h"

#include <sim/constants.h>
#include <simlib/filesystem.h>

namespace job_handlers {

void DeleteInternalFile::run() {
	STACK_UNWINDING_MARK;

	job_log("Internal file ID: ", internal_file_id);
	(void)unlink(internal_file_path(internal_file_id));

	auto transaction = mysql.start_transaction();
	// The internal_file may already be deleted
	mysql.prepare("DELETE FROM internal_files WHERE id=?")
	   .bindAndExecute(internal_file_id);

	job_done();

	transaction.commit();
}

} // namespace job_handlers
