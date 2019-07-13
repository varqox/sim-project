#include "job_handler.h"
#include "../main.h"

#include <sim/constants.h>

namespace job_handlers {

void JobHandler::job_canceled() {
	STACK_UNWINDING_MARK;

	mysql
	   .prepare("UPDATE jobs SET status=" JSTATUS_CANCELED_STR ", data=?"
	            " WHERE id=?")
	   .bindAndExecute(get_log(), job_id_);
}

void JobHandler::job_done() {
	STACK_UNWINDING_MARK;

	mysql
	   .prepare("UPDATE jobs SET status=" JSTATUS_DONE_STR ", data=?"
	            " WHERE id=?")
	   .bindAndExecute(get_log(), job_id_);
}

void JobHandler::job_done(StringView new_info) {
	STACK_UNWINDING_MARK;

	mysql
	   .prepare("UPDATE jobs SET status=" JSTATUS_DONE_STR ", info=?, data=?"
	            " WHERE id=?")
	   .bindAndExecute(new_info, get_log(), job_id_);
}

} // namespace job_handlers
