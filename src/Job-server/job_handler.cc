#include "job_handler.h"
#include "main.h"

#include <sim/constants.h>

void JobHandler::job_done() {
	STACK_UNWINDING_MARK;

	mysql.prepare("UPDATE jobs SET status=" JSTATUS_DONE_STR ", data=?"
		" WHERE id=?").bindAndExecute(get_log(), job_id);
}
