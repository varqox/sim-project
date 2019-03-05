#include <sim/jobs.h>
#include <simlib/time.h>

namespace jobs {

void restart_job(MySQL::Connection& mysql, StringView job_id, JobType job_type,
	StringView job_info, bool notify_job_server)
{
	STACK_UNWINDING_MARK;
	using JT = JobType;

	// Restart adding / reuploading problem
	bool adding = isOneOf(job_type, JT::ADD_PROBLEM,
		JT::ADD_JUDGE_MODEL_SOLUTION);
	bool reupload = isOneOf(job_type, JT::REUPLOAD_PROBLEM,
		JT::REUPLOAD_JUDGE_MODEL_SOLUTION);

	if (adding or reupload) {
		jobs::AddProblemInfo info {job_info};
		info.stage = jobs::AddProblemInfo::FIRST;

		auto transaction = mysql.start_transaction();

		// Delete temporary files created during problem adding
		mysql.prepare("INSERT INTO jobs(file_id, creator, type, priority,"
				" status, added, aux_id, info, data)"
			" SELECT tmp_file_id, NULL, " JTYPE_DELETE_FILE_STR ", ?, "
				JSTATUS_PENDING_STR ", ?, NULL, '', '' FROM jobs WHERE id=?"
				" AND tmp_file_id IS NOT NULL")
			.bindAndExecute(priority(JobType::DELETE_FILE), mysql_date(), job_id);

		// Restart job
		mysql.prepare("UPDATE jobs"
			" SET type=?, status=" JSTATUS_PENDING_STR ", tmp_file_id=NULL,"
				" info=? WHERE id=?")
			.bindAndExecute(uint(adding ? JT::ADD_PROBLEM : JT::REUPLOAD_PROBLEM),
				info.dump(), job_id);

		transaction.commit();

	// Restart job of other type
	} else {
		mysql.prepare("UPDATE jobs SET status=" JSTATUS_PENDING_STR
			" WHERE id=?").bindAndExecute(job_id);
	}

	if (notify_job_server)
		jobs::notify_job_server();
}


void restart_job(MySQL::Connection& mysql, StringView job_id,
	bool notify_job_server)
{
	uint8_t jtype;
	InplaceBuff<128> jinfo;
	auto stmt = mysql.prepare("SELECT type, info FROM jobs WHERE id=?");
	stmt.res_bind_all(jtype, jinfo);
	stmt.bindAndExecute(job_id);
	if (stmt.next())
		restart_job(mysql, job_id, JobType(jtype), jinfo, notify_job_server);
}

} // namespace jobs
