#include <sim/jobs.h>

namespace jobs {

void restart_job(MySQL::Connection& mysql, StringView job_id, JobType job_type,
	StringView job_info, bool notify_job_server)
{
	STACK_UNWINDING_MARK;
	using JT = JobType;

	// Restart adding / reuploading problem
	bool adding = isIn(job_type,
		{JT::ADD_PROBLEM, JT::ADD_JUDGE_MODEL_SOLUTION});
	bool reupload = isIn(job_type,
		{JT::REUPLOAD_PROBLEM, JT::REUPLOAD_JUDGE_MODEL_SOLUTION});

	if (adding or reupload) {
		jobs::AddProblemInfo info {job_info};
		info.stage = jobs::AddProblemInfo::FIRST;

		auto stmt = mysql.prepare("UPDATE jobs"
			" SET type=?, status=" JSTATUS_PENDING_STR ", info=? WHERE id=?");

		stmt.bindAndExecute(
			uint(adding ? JT::ADD_PROBLEM : JT::REUPLOAD_PROBLEM), info.dump(),
			job_id);

	// Restart job of other type
	} else {
		auto stmt = mysql.prepare("UPDATE jobs SET status=" JSTATUS_PENDING_STR
			" WHERE id=?");
		stmt.bindAndExecute(job_id);
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
