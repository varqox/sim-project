#include "main.h"

#include <sim/constants.h>
#include <sim/file.h>
#include <sim/submission.h>

void contest_problem_reselect_final_submissions(uint64_t job_id,
	StringView contest_problem_id)
{
	// Lock the table to be able to safely modify the submission
	// locking contest_problems is required by update_final()
	mysql.update("LOCK TABLES submissions WRITE, contest_problems READ");
	auto lock_guard = make_call_in_destructor([&]{
		mysql.update("UNLOCK TABLES");
	});

	auto stmt = mysql.prepare("SELECT owner, problem_id FROM submissions"
		" WHERE contest_problem_id=? AND contest_final=1");
	stmt.bindAndExecute(contest_problem_id);

	InplaceBuff<30> sowner;
	InplaceBuff<30> problem_id;
	stmt.res_bind_all(sowner, problem_id);
	while (stmt.next())
		submission::update_final(mysql, sowner, problem_id, contest_problem_id, false);

	// Done
	lock_guard.call_and_cancel();
	stmt = mysql.prepare("UPDATE jobs"
		" SET status=" JSTATUS_DONE_STR ", data='' WHERE id=?");
	stmt.bindAndExecute(job_id);
}

void delete_contest(uint64_t job_id, StringView contest_id) {
	InplaceBuff<256> job_log;

	auto set_failure = [&](auto&&... args) {
		job_log.append(std::forward<decltype(args)>(args)...);
		auto stmt = mysql.prepare("UPDATE jobs"
			" SET status=" JSTATUS_FAILED_STR ", data=?"
			" WHERE id=? AND status!=" JSTATUS_CANCELED_STR);
		stmt.bindAndExecute(job_log, job_id);

		stdlog("Job ", job_id, ":\n", job_log);
	};

	// TODO: create transaction - check contest information, add jobs to delete submission files (when the global file-system will be created, and then delete the contest with all consequent actions)

	// Log some info about the deleted contest
	{
		auto stmt = mysql.prepare("SELECT name FROM contests WHERE id=?");
		stmt.bindAndExecute(contest_id);
		InplaceBuff<32> cname;
		stmt.res_bind_all(cname);
		if (not stmt.next()) {
			return set_failure("Contest with id: ", contest_id,
			" does not exist");
		}

		job_log.append("Contest: ", cname, " (", contest_id, ")\n");
	}

	// Delete contest (all necessary actions will take place thanks to foreign
	// key constrains)
	mysql.prepare("DELETE FROM contests WHERE id=?").bindAndExecute(contest_id);

	mysql.prepare("UPDATE jobs SET status=" JSTATUS_DONE_STR ", data=?"
		" WHERE id=?").bindAndExecute(job_log, job_id);
}

void delete_contest_round(uint64_t job_id, StringView contest_round_id) {
	InplaceBuff<256> job_log;

	auto set_failure = [&](auto&&... args) {
		job_log.append(std::forward<decltype(args)>(args)...);
		auto stmt = mysql.prepare("UPDATE jobs"
			" SET status=" JSTATUS_FAILED_STR ", data=?"
			" WHERE id=? AND status!=" JSTATUS_CANCELED_STR);
		stmt.bindAndExecute(job_log, job_id);

		stdlog("Job ", job_id, ":\n", job_log);
	};

	// TODO: create transaction - check contest round information, add jobs to delete submission files (when the global file-system will be created, and then delete the contest round with all consequent actions)

	// Log some info about the deleted contest round
	{
		auto stmt = mysql.prepare("SELECT c.name, c.id, r.name"
			" FROM contest_rounds r"
			" JOIN contests c ON c.id=r.contest_id"
			" WHERE r.id=?");
		stmt.bindAndExecute(contest_round_id);
		InplaceBuff<32> cname, cid, rname;
		stmt.res_bind_all(cname, cid, rname);
		if (not stmt.next()) {
			return set_failure("Contest round with id: ", contest_round_id,
				" does not exist or the contest hierarchy is broken (likely the"
				" former).");
		}

		job_log.append("Contest: ", cname, " (", cid, ")\n"
			"Contest round: ", rname, " (", contest_round_id, ")\n");
	}

	// Delete contest round (all necessary actions will take place thanks to
	// foreign key constrains)
	mysql.prepare("DELETE FROM contest_rounds WHERE id=?")
		.bindAndExecute(contest_round_id);

	mysql.prepare("UPDATE jobs SET status=" JSTATUS_DONE_STR ", data=?"
		" WHERE id=?").bindAndExecute(job_log, job_id);
}

void delete_contest_problem(uint64_t job_id, StringView contest_problem_id) {
	InplaceBuff<256> job_log;

	auto set_failure = [&](auto&&... args) {
		job_log.append(std::forward<decltype(args)>(args)...);
		auto stmt = mysql.prepare("UPDATE jobs"
			" SET status=" JSTATUS_FAILED_STR ", data=?"
			" WHERE id=? AND status!=" JSTATUS_CANCELED_STR);
		stmt.bindAndExecute(job_log, job_id);

		stdlog("Job ", job_id, ":\n", job_log);
	};

	// TODO: create transaction - check contest problem information, add jobs to delete submission files (when the global file-system will be created, and then delete the contest round with all consequent actions)

	// Log some info about the deleted contest problem
	{
		auto stmt = mysql.prepare("SELECT c.name, c.id, r.name, r.id, cp.name,"
				" p.name, p.id"
			" FROM contest_problems cp"
			" JOIN contest_rounds r ON r.id=cp.contest_round_id"
			" JOIN contests c ON c.id=cp.contest_id"
			" JOIN problems p ON p.id=cp.problem_id"
			" WHERE cp.id=?");
		stmt.bindAndExecute(contest_problem_id);
		InplaceBuff<32> cname, cid, rname, rid, cpname, pname, pid;
		stmt.res_bind_all(cname, cid, rname, rid, cpname, pname, pid);
		if (not stmt.next()) {
			return set_failure("Contest problem with id: ", contest_problem_id,
				" does not exist or the contest hierarchy is broken (likely the"
				" former).");
		}

		job_log.append("Contest: ", cname, " (", cid, ")\n"
			"Contest round: ", rname, " (", rid, ")\n"
			"Contest problem: ", cpname, " (", contest_problem_id, ")\n"
			"Attached problem: ", pname, " (", pid, ")\n");
	}

	// Delete contest problem (all necessary actions will take place thanks to
	// foreign key constrains)
	mysql.prepare("DELETE FROM contest_problems WHERE id=?")
		.bindAndExecute(contest_problem_id);

	mysql.prepare("UPDATE jobs SET status=" JSTATUS_DONE_STR ", data=?"
		" WHERE id=?").bindAndExecute(job_log, job_id);
}
