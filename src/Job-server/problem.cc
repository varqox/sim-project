#include "main.h"

#include <deque>
#include <sim/jobs.h>
#include <sim/submission.h>
#include <simlib/libzip.h>
#include <simlib/process.h>
#include <simlib/sim/conver.h>
#include <simlib/sim/judge_worker.h>
#include <simlib/sim/problem_package.h>
#include <simlib/spawner.h>

using jobs::AddProblemInfo;
using std::pair;
using std::string;
using std::vector;

namespace {

enum Action {
	ADDING,
	REUPLOADING
};

} // anonymous namespace

// Before judging the model solution
template<Action action>
static void first_stage(uint64_t job_id, AddProblemInfo& info) {
	STACK_UNWINDING_MARK;

	auto source_package = concat<PATH_MAX>("jobs_files/", job_id, ".zip");
	auto dest_package = concat<PATH_MAX>("jobs_files/", job_id, ".zip.prep");

	FileRemover package_remover(dest_package.to_cstr());

	sim::Conver::ReportBuff job_log;
	job_log.append("Stage: FIRST");

	auto set_failure = [&](auto&&... args) {
		job_log.append(std::forward<decltype(args)>(args)...);
		auto stmt = mysql.prepare("UPDATE jobs"
			" SET status=" JSTATUS_FAILED_STR ", data=?"
			" WHERE id=? AND status!=" JSTATUS_CANCELED_STR);
		stmt.bindAndExecute(job_log.str, job_id);

		stdlog("Job ", job_id, ":\n", job_log.str);
	};

	// dest_package is initially equal to the source_package
	(void)remove(dest_package);
	copy(source_package, dest_package);

	/* Construct Simfile */

	sim::Conver conver;
	conver.setPackagePath(dest_package.to_string());

	// Set Conver options
	sim::Conver::Options copts;
	copts.name = info.name;
	copts.label = info.label;
	copts.memory_limit = info.memory_limit;
	copts.global_time_limit = info.global_time_limit;
	copts.max_time_limit = PROBLEM_MAX_TIME_LIMIT;
	copts.reset_time_limits_using_model_solution = info.reset_time_limits;
	copts.ignore_simfile = info.ignore_simfile;
	copts.seek_for_new_tests = info.seek_for_new_tests;
	copts.reset_scoring = info.reset_scoring;
	copts.require_statement = true;

	sim::Conver::ConstructionResult cr;
	try {
		cr = conver.constructSimfile(copts);
	} catch (const std::exception& e) {
		return set_failure(conver.getReport(), "Conver failed: ", e.what());
	}

	// Check problem's name's length
	if (cr.simfile.name.size() > PROBLEM_NAME_MAX_LEN) {
		return set_failure("Problem's name is too long (max allowed length: ",
			PROBLEM_NAME_MAX_LEN, ')');
	}

	// Check problem's label's length
	if (cr.simfile.label.size() > PROBLEM_LABEL_MAX_LEN)
		return set_failure("Problem's label is too long (max allowed length: ",
			PROBLEM_LABEL_MAX_LEN, ')');

	job_log.append(conver.getReport());

	/* Advance the stage */

	// Put the Simfile into the package
	ZipFile zip(dest_package);
	{
		auto simfile_str = cr.simfile.dump();
		zip.file_add(concat(cr.pkg_master_dir, "Simfile"),
			zip.source_buffer(simfile_str), ZIP_FL_OVERWRITE);
		zip.close();
	}

	info.stage = AddProblemInfo::SECOND;

	decltype(mysql.prepare("")) stmt;
	switch (cr.status) {
	case sim::Conver::Status::COMPLETE: {
		// Advance the job to the SECOND stage
		stmt = mysql.prepare("UPDATE jobs"
			" SET status=" JSTATUS_PENDING_STR ", info=?, data=?"
			" WHERE id=? AND status!=" JSTATUS_CANCELED_STR);
		stmt.bindAndExecute(info.dump(), job_log.str, job_id);
		break;
	}
	case sim::Conver::Status::NEED_MODEL_SOLUTION_JUDGE_REPORT: {
		// Transform the job into a JUDGE_MODEL_SOLUTION job
		stmt = mysql.prepare("UPDATE jobs"
			" SET type=?, status=" JSTATUS_PENDING_STR ", info=?, data=?"
			" WHERE id=? AND status!=" JSTATUS_CANCELED_STR);
		stmt.bindAndExecute(
			uint(action == Action::ADDING ? JobType::ADD_JUDGE_MODEL_SOLUTION
				: JobType::REUPLOAD_JUDGE_MODEL_SOLUTION),
			info.dump(), job_log.str, job_id);
		break;
	}
	}

	stdlog("Job ", job_id, ":\n", job_log.str);
	if (stmt.affected_rows() > 0)
		package_remover.cancel(); // The job was not canceled
}

/**
 * @brief Continue adding the problem after judging the model solution
 * @details It leaves add problem and its submissions with type set to VOID
 *
 * @param job_id job id
 * @param job_owner job owner
 * @param info add info
 * @param job_log job_log to append the log to
 * @tparam Trait trait to use
 *
 * @return Id of the added problem
 *
 * @errors If any error occurs an appropriate exception is thrown
 */
template<Action action>
static uint64_t second_stage(uint64_t job_id, StringView job_owner,
	const AddProblemInfo&, sim::Conver::ReportBuff& job_log)
{
	STACK_UNWINDING_MARK;

	auto package_path = concat("jobs_files/", job_id, ".zip.prep");
	FileRemover job_package_remover(package_path.to_cstr());

	job_log.append("\nStage: SECOND");
	auto pkg_master_dir = sim::zip_package_master_dir(package_path);

	// Load the Simfile
	ZipFile zip(package_path, ZIP_RDONLY);
	string simfile_str =
		zip.extract_to_str(zip.get_index(concat(pkg_master_dir, "Simfile")));

	sim::Simfile sf {simfile_str};
	sf.loadName();
	sf.loadLabel();
	sf.loadSolutions();

	// Add the problem to the database
	InplaceBuff<64> current_date = concat<64>(mysql_date());
	auto stmt = mysql.prepare(
		"INSERT problems (type, name, label, simfile, owner, added,"
			" last_edit)"
		" VALUES(" PTYPE_VOID_STR ", ?, ?, ?, ?, ?, ?)");
	stmt.bindAndExecute(sf.name, sf.label, std::move(simfile_str), job_owner,
		current_date, current_date);

	// Obtain the new problem's id
	uint64_t problem_id = stmt.insert_id();
	auto pid_str = toStr(problem_id);

	/* Begin transaction */

	// SQLite transaction is handled in the function add_problem() that calls
	// this function so we don't have to worry about that
	auto rollbacker = [&] {
		SignalBlocker sb; // Prevent the transaction rollback from interruption

		// Delete problem's files
		(void)remove(concat("problems/", pid_str, ".zip"));

		// Delete the problem and its submissions
		stmt = mysql.prepare("DELETE FROM problems WHERE id=?");
		stmt.bindAndExecute(problem_id);

		// Delete submissions
		{
			stmt = mysql.prepare("SELECT id FROM submissions"
				" WHERE problem_id=?");
			stmt.bindAndExecute(problem_id);
			InplaceBuff<20> submission_id;
			stmt.res_bind_all(submission_id);
			while (stmt.next())
				submission::delete_submission(mysql, submission_id);
		}
	};
	CallInDtor<decltype(rollbacker)> rollback_maker {rollbacker};

	// Insert the problem into the SQLite FTS5 table `problems`
	// SQLite::Statement sqlite_stmt {sqlite.prepare(
		// "INSERT INTO problems (rowid, type, name, label)"
		// " VALUES(?, ?, ?, ?)")};
	// sqlite_stmt.bindInt64(1, problem_id);
	// sqlite_stmt.bindInt(2, EnumVal<ProblemType>(info.problem_type).int_val());
	// sqlite_stmt.bindText(3, sf.name, SQLITE_STATIC);
	// sqlite_stmt.bindText(4, sf.label, SQLITE_STATIC);
	// throw_assert(sqlite_stmt.step() == SQLITE_DONE);

	// Move the package to its destination
	auto package_dest = concat("problems/", pid_str, ".zip");
	if (move(package_path, package_dest))
		THROW("move() failed", errmsg());

	job_package_remover.cancel();

	auto fname_to_lang = [](StringView extension) {
		auto lang = sim::filename_to_lang(extension);
		switch (lang) {
		case sim::SolutionLanguage::C: return SubmissionLanguage::C;
		case sim::SolutionLanguage::CPP: return SubmissionLanguage::CPP;
		case sim::SolutionLanguage::PASCAL: return SubmissionLanguage::PASCAL;
		case sim::SolutionLanguage::UNKNOWN: THROW("Not supported language");
		}

		throw_assert(false);
	};

	// Submit the solutions
	job_log.append("Submitting solutions...");
	const string zero_date = mysql_date(0);
	EnumVal<SubmissionLanguage> lang;
	stmt = mysql.prepare("INSERT submissions (owner, problem_id,"
			" contest_problem_id, contest_round_id, contest_id, type, language,"
			" initial_status, full_status, submit_time, last_judgment,"
			" initial_report, final_report)"
		" VALUES(NULL, ?, NULL, NULL, NULL, "
			STYPE_VOID_STR ", ?, " SSTATUS_PENDING_STR ", " SSTATUS_PENDING_STR
			", ?, ?, '', '')");
	stmt.bind_all(problem_id, lang, current_date, zero_date);

	for (auto&& solution : sf.solutions) {
		job_log.append("Submit: ", solution);

		current_date = mysql_date();
		lang = fname_to_lang(solution);
		stmt.execute();
		uint64_t submission_id = mysql.insert_id();

		// Save the submission source code
		zip.extract_to_file(zip.get_index(concat(pkg_master_dir, solution)),
			concat("solutions/", submission_id));
	}
	job_log.append("Done.");

	// Notify the job-server that there are submissions to judge
	utime(JOB_SERVER_NOTIFYING_FILE, nullptr);

	// TODO: fix updating submission status so that it will work with PROBLEM_SOLUTION submissions and other queries associated with this problem

	// End transaction
	rollback_maker.cancel();

	return problem_id;
}

/**
 * @brief Adds problem to the problemset, submits its solutions and calls
 *   @p success_callback. Note that this function does not make the problem
 *   visible via the web-interface.
 *
 * @param job_id job id
 * @param job_owner job owner
 * @param aux_id job's auxiliary id
 * @param info problem info
 * @param success_callback function to call just after finishing adding the problem
 * @tparam Trait trait to use
*/
template<Action action, class Func>
static void add_problem(uint64_t job_id, StringView job_owner, StringView aux_id,
	AddProblemInfo& info, Func&& success_callback)
{
	STACK_UNWINDING_MARK;

	switch (info.stage) {
	case AddProblemInfo::FIRST:
		first_stage<action>(job_id, info);
		break;

	case AddProblemInfo::SECOND: {
		sim::Conver::ReportBuff job_log;
		auto stmt = mysql.prepare("UPDATE jobs"
			" SET status=?, aux_id=?, data=CONCAT(data,?) WHERE id=?");
		EnumVal<JobStatus> status; // [[maybe_uninitilized]]
		uint64_t apid; // Added problem's id
		try {
			// sqlite.execute("BEGIN");
			apid = second_stage<action>(job_id, job_owner, info, job_log);
			success_callback(apid, job_log);
			// sqlite.execute("COMMIT");

			status = JobStatus::DONE;
			if (action == Action::ADDING)
				stmt.bind(1, apid);
			else
				stmt.bind(1, aux_id);

		} catch (const std::exception& e) {
			// To avoid exceptions C API is used
			// (void)sqlite3_exec(sqlite, "ROLLBACK", nullptr, nullptr, nullptr);

			ERRLOG_CATCH(e);
			job_log.append("Caught exception: ", e.what());

			status = JobStatus::FAILED;
			if (action == Action::REUPLOADING)
				stmt.bind(1, aux_id);
			else
				stmt.bind(1, nullptr);

			// Remove temporary package
			remove(concat("jobs_files/", job_id, ".zip.prep"));
		}

		stmt.bind(0, status);
		stmt.bind(2, job_log.str);
		stmt.bind(3, job_id);
		stmt.fixAndExecute();

		stdlog("Job: ", job_id, '\n', job_log.str);
		break;
	}
	}
}

static void add_jobs_to_judge_problem_solutions(uint64_t problem_id) {
	STACK_UNWINDING_MARK;

	uint64_t submission_id;
	auto stmt = mysql.prepare("SELECT id FROM submissions"
		" WHERE type=" STYPE_PROBLEM_SOLUTION_STR " AND problem_id=?");
	stmt.res_bind_all(submission_id);
	stmt.bindAndExecute(problem_id);

	// Create a job to judge the submission
	auto insert_stmt = mysql.prepare("INSERT jobs (creator, status,"
			" priority, type, added, aux_id, info, data)"
		" VALUES(NULL, " JSTATUS_PENDING_STR ", ?, "
			JTYPE_JUDGE_SUBMISSION_STR ", ?, ?, ?, '')");
	// Problem's solutions are more important than the ordinary submissions
	constexpr uint prio = priority(JobType::JUDGE_SUBMISSION) + 1;
	const auto current_date = mysql_date();
	const auto info = jobs::dumpString(
		intentionalUnsafeStringView(toStr(problem_id)));
	insert_stmt.bind_all(prio, current_date, submission_id, info);

	while (stmt.next())
		insert_stmt.execute();

	jobs::notify_job_server();
}

void add_problem(uint64_t job_id, StringView job_owner, StringView info) {
	STACK_UNWINDING_MARK;

	AddProblemInfo p_info {info};
	add_problem<Action::ADDING>(job_id, job_owner, "", p_info,
		[&](uint64_t problem_id, sim::Conver::ReportBuff&) {
			STACK_UNWINDING_MARK;

			// Make the problem and its solutions' submissions public
			auto stmt = mysql.prepare("UPDATE problems p, submissions s"
				" SET p.type=?, s.type=" STYPE_PROBLEM_SOLUTION_STR
				" WHERE p.id=? AND s.problem_id=?");
			stmt.bindAndExecute(EnumVal<ProblemType>(p_info.problem_type),
				problem_id, problem_id);

			add_jobs_to_judge_problem_solutions(problem_id);
		});

}

void reupload_problem(uint64_t job_id, StringView job_owner, StringView info,
	StringView problem_id)
{
	STACK_UNWINDING_MARK;

	AddProblemInfo p_info {info};
	add_problem<Action::REUPLOADING>(job_id, job_owner, problem_id, p_info,
		[&](uint64_t tmp_problem_id, sim::Conver::ReportBuff& job_log) {
			STACK_UNWINDING_MARK;

			job_log.append("Replacing the old package with the new one...");

			// Make a backup of the problem
			vector<pair<bool, string>> pbackup;
			auto stmt = mysql.prepare("SELECT * FROM problems"
				" WHERE id=?");
			stmt.bindAndExecute(problem_id);
			uint n = stmt.fields_num();

			InplaceArray<InplaceBuff<65536>, 16> buff {n};
			InplaceArray<my_bool, 16> is_null {n};
			for (uint i = 0; i < n; ++i) {
				stmt.res_bind(i, buff[i]);
				stmt.res_bind_isnull(i, is_null[i]);
			}
			stmt.resFixBinds();

			string prob_restore_sql;
			StringView previous_owner;
			if (stmt.next()) {
				prob_restore_sql = "REPLACE problems VALUES(";
				pbackup.resize(n);
				for (uint i = 0; i < n; ++i) {
					pbackup[i] = {is_null[i], buff[i].to_string()};
					prob_restore_sql += "?,";
				}

				prob_restore_sql.back() = ')';

				// Save the previous owner
				constexpr uint OWNER_IDX = 5;
				throw_assert("owner" == StringView{
					mysql_fetch_field_direct(stmt.resMetadata().get(),
						OWNER_IDX)->name});
				previous_owner = pbackup[OWNER_IDX].second;
				// Commit the change to the database
				stmt = mysql.prepare("UPDATE jobs SET info=? WHERE id=?");
				stmt.bindAndExecute(p_info.dump(), job_id);
			}

			// Make a backup of the problem's solutions' submissions
			vector<vector<pair<bool, string>>> sol_backup;
			string srestore_sql;
			stmt = mysql.prepare("SELECT * FROM submissions"
				" WHERE type=" STYPE_PROBLEM_SOLUTION_STR " AND problem_id=?");
			stmt.bindAndExecute(problem_id);
			if (stmt.next()) {
				n = stmt.fields_num();
				// Restore sql
				srestore_sql = "REPLACE submissions VALUES(";
				for (uint i = 0; i < n; ++i)
					srestore_sql += "?,";
				srestore_sql.back() = ')';

				// Backup the submissions
				buff.resize(n);
				is_null.resize(n);

				for (uint i = 0; i < n; ++i) {
					stmt.res_bind(i, buff[i]);
					stmt.res_bind_isnull(i, is_null[i]);
				}
				stmt.resFixBinds();

				do {
					sol_backup.emplace_back();
					sol_backup.back().resize(n);
					for (uint i = 0; i < n; ++i)
						sol_backup.back()[i] =
							{is_null[i], buff[i].to_string()};

				} while (stmt.next());
			}

			auto problem_path = concat<PATH_MAX>("problems/", problem_id, ".zip");
			auto backuped_problem = concat<PATH_MAX>(problem_path, ".backup");

			(void)remove(backuped_problem);

			ThreadSignalBlocker sb; // This part cannot be interrupted
			STACK_UNWINDING_MARK;

			// Backup old problem's files
			if (rename(problem_path, backuped_problem))
				THROW("rename() failed", errmsg());

			auto backup_restorer = make_call_in_destructor([&]{
				// Restore problem's files
				(void)rename(backuped_problem, problem_path);

				// Restore the problem in the database
				if (prob_restore_sql.size()) {
					stmt = mysql.prepare(prob_restore_sql);
					for (uint i = 0; i < pbackup.size(); ++i) {
						if (pbackup[i].first)
							stmt.bind(i, nullptr);
						else
							stmt.bind(i, pbackup[i].second);
					}
					stmt.fixAndExecute();
				}

				// Restore problem's solutions' submissions in the database
				if (srestore_sql.size()) {
					stmt = mysql.prepare(srestore_sql);
					for (auto&& v : sol_backup) {
						for (uint i = 0; i < v.size(); ++i) {
							if (v[i].first)
								stmt.bind(i, nullptr);
							else
								stmt.bind(i, v[i].second);
						}
						stmt.fixAndExecute();
					}
				}
			});

			stmt = mysql.prepare("SELECT added FROM problems WHERE id=?");
			stmt.bindAndExecute(problem_id);
			InplaceBuff<32> added;
			stmt.res_bind_all(added);
			throw_assert(stmt.next());

			// Delete the old problem
			stmt = mysql.prepare("DELETE FROM problems WHERE id=?");
			stmt.bindAndExecute(problem_id);

			// Replace the old problem with the new one
			stmt = mysql.prepare("UPDATE problems"
				" SET id=?, type=?, owner=?, added=? WHERE id=?");
			stmt.bindAndExecute(problem_id,
				EnumVal<ProblemType>(p_info.problem_type),
				(previous_owner.empty() ? job_owner : previous_owner),
				added, tmp_problem_id);

			// Replace packages
			if (rename(concat("problems/", tmp_problem_id, ".zip"),
				problem_path))
			{
				THROW("rename() failed", errmsg());
			}

			// Replace the problem in the SQLite FTS5 table `problems`
			// SQLite::Statement sqlite_stmt {sqlite.prepare(
				// "UPDATE OR REPLACE problems SET rowid=? WHERE rowid=?")};
			// sqlite_stmt.bindText(1, problem_id, SQLITE_STATIC);
			// sqlite_stmt.bindInt64(2, tmp_problem_id);
			// throw_assert(sqlite_stmt.step() == SQLITE_DONE);

			// Delete the old problem's solutions' submissions
			{
				stmt = mysql.prepare("SELECT id FROM submissions"
					" WHERE type=" STYPE_PROBLEM_SOLUTION_STR
						" AND problem_id=?");
				stmt.bindAndExecute(problem_id);
				InplaceBuff<20> submission_id;
				stmt.res_bind_all(submission_id);
				while (stmt.next())
					submission::delete_submission(mysql, submission_id);
			}

			// Activate the new problem's solutions' submissions
			stmt = mysql.prepare("UPDATE submissions"
				" SET problem_id=?, type=" STYPE_PROBLEM_SOLUTION_STR
				" WHERE problem_id=?");
			stmt.bindAndExecute(problem_id, tmp_problem_id);

			add_jobs_to_judge_problem_solutions(strtoull(problem_id));

			backup_restorer.cancel();
			(void)remove(backuped_problem);
		});
}

void delete_problem(uint64_t job_id, StringView problem_id) {
	sim::Conver::ReportBuff job_log;

	auto set_failure = [&](auto&&... args) {
		job_log.append(std::forward<decltype(args)>(args)...);
		mysql.update("UNLOCK TABLES");
		auto stmt = mysql.prepare("UPDATE jobs"
			" SET status=" JSTATUS_FAILED_STR ", data=?"
			" WHERE id=? AND status!=" JSTATUS_CANCELED_STR);
		stmt.bindAndExecute(job_log.str, job_id);

		stdlog("Job ", job_id, ":\n", job_log.str);
	};

	// Lock the tables to be able to safely delete problem
	{
		mysql.update("LOCK TABLES problems WRITE, submissions WRITE,"
			" contest_problems READ");
		auto lock_guard = make_call_in_destructor([&]{
			mysql.update("UNLOCK TABLES");
		});

		// Check whether the problem is not used as contest problem
		{
			auto stmt = mysql.prepare("SELECT 1 FROM contest_problems"
				" WHERE problem_id=? LIMIT 1");
			stmt.bindAndExecute(problem_id);
			if (stmt.next()) {
				lock_guard.call_and_cancel();
				return set_failure("There exists a contest problem that uses"
					" (attaches) this problem. You have to delete all of them"
					" to be able to delete this problem.");
			}
		}

		// Assure that the problems exist and log its Simfile
		{
			auto stmt = mysql.prepare("SELECT simfile FROM problems"
				" WHERE id=? AND type!=" PTYPE_VOID_STR);
			stmt.bindAndExecute(problem_id);
			InplaceBuff<1> simfile;
			stmt.res_bind_all(simfile);
			if (not stmt.next())
				return set_failure("Problem does not exist");

			job_log.append("Deleted problem Simfile:\n", simfile);
		}

		// Delete submissions
		{
			auto stmt = mysql.prepare("SELECT id FROM submissions"
				" WHERE problem_id=?");
			stmt.bindAndExecute(problem_id);
			InplaceBuff<20> submission_id;
			stmt.res_bind_all(submission_id);
			while (stmt.next())
				submission::delete_submission(mysql, submission_id);
		}

		auto stmt = mysql.prepare("DELETE FROM problems WHERE id=?");
		stmt.bindAndExecute(problem_id);
	}

	// Delete problem tags
	auto stmt = mysql.prepare("DELETE FROM problem_tags WHERE problem_id=?");
	stmt.bindAndExecute(problem_id);

	// Delete problem's files
	(void)remove_r(concat("problems/", problem_id));
	(void)remove(concat("problems/", problem_id, ".zip"));

	stmt = mysql.prepare("UPDATE jobs SET status=" JSTATUS_DONE_STR
		", data=? WHERE id=?");
	stmt.bindAndExecute(job_log.str, job_id);
}

void merge_problem_into_another(uint64_t job_id, StringView problem_id,
	jobs::MergeProblemsInfo info)
{
	sim::Conver::ReportBuff job_log;

	auto set_failure = [&](auto&&... args) {
		job_log.append(std::forward<decltype(args)>(args)...);
		// mysql.update("UNLOCK TABLES"); // Unneeded as the jobs table is write-locked
		auto stmt = mysql.prepare("UPDATE jobs"
			" SET status=" JSTATUS_FAILED_STR ", data=?"
			" WHERE id=? AND status!=" JSTATUS_CANCELED_STR);
		stmt.bindAndExecute(job_log.str, job_id);

		stdlog("Job ", job_id, ":\n", job_log.str);
	};

	// Lock the tables to be able to safely transfer problem usages
	{
		mysql.update("LOCK TABLES problems WRITE, contest_problems WRITE,"
			" submissions WRITE, jobs WRITE");
		auto lock_guard = make_call_in_destructor([&]{
			mysql.update("UNLOCK TABLES");
		});

		// Assure that both problems exist
		{
			auto stmt = mysql.prepare("SELECT simfile FROM problems"
				" WHERE id=? AND type!=" PTYPE_VOID_STR);
			stmt.bindAndExecute(problem_id);
			InplaceBuff<1> simfile;
			stmt.res_bind_all(simfile);
			if (not stmt.next())
				return set_failure("Problem does not exist");

			job_log.append("Merged problem Simfile:\n", simfile);

			stmt = mysql.prepare("SELECT 1 FROM problems"
				" WHERE id=? AND type!=" PTYPE_VOID_STR);
			stmt.bindAndExecute(info.target_problem_id);
			if (not stmt.next())
				return set_failure("Target problem does not exist");
		}

		// Transfer contest problems
		{
			auto stmt = mysql.prepare("UPDATE contest_problems SET problem_id=?"
				" WHERE problem_id=?");
			stmt.bindAndExecute(info.target_problem_id, problem_id);
		}

		// Delete problem solutions
		{
			auto stmt = mysql.prepare("SELECT id FROM submissions"
				" WHERE problem_id=? AND type=" STYPE_PROBLEM_SOLUTION_STR);
			stmt.bindAndExecute(problem_id);
			InplaceBuff<20> submission_id;
			stmt.res_bind_all(submission_id);
			while (stmt.next())
				submission::delete_submission(mysql, submission_id);
		}

		// Collect update finals
		std::deque<std::array<MySQL::Optional<int64_t>, 2>> finals_to_update;
		{
			auto stmt = mysql.prepare("SELECT DISTINCT owner, contest_problem_id"
				" FROM submissions WHERE problem_id=?");
			stmt.bindAndExecute(problem_id);
			std::array<MySQL::Optional<int64_t>, 2> ftu_elem {};
			stmt.res_bind_all(ftu_elem[0], ftu_elem[1]);
			while (stmt.next())
				finals_to_update.emplace_back(ftu_elem);
		}

		// Schedule rejudge of the transferred submissions
		if (info.rejudge_transferred_submissions) {
			// Safe as the jobs table is locked and jobs server won't read the jobs
			auto stmt = mysql.prepare("INSERT INTO jobs(creator, status,"
					" priority, type, added, aux_id, info, data)"
				" SELECT NULL, " JSTATUS_PENDING_STR ", ?, "
					JTYPE_REJUDGE_SUBMISSION_STR ", ?, id, ?, ''"
				" FROM submissions WHERE problem_id=? ORDER BY id");
			stmt.bindAndExecute(
				priority(JobType::REJUDGE_SUBMISSION),
				mysql_date(),
				jobs::dumpString(intentionalUnsafeStringView(toStr(info.target_problem_id))),
				problem_id);
		}

		// Transfer problem submissions that are not problem solutions
		{
			auto stmt = mysql.prepare("UPDATE submissions SET problem_id=?"
				" WHERE problem_id=?");
			stmt.bindAndExecute(info.target_problem_id, problem_id);
		}

		// Update finals
		for (auto const& ftu_elem : finals_to_update) {
			InplaceBuff<32> owner, contest_problem_id;
			if (ftu_elem[0].has_value())
				owner = toStr(*ftu_elem[0]);
			if (ftu_elem[1].has_value())
				contest_problem_id = toStr(*ftu_elem[1]);

			submission::update_final(mysql, owner,
				intentionalUnsafeStringView(toStr(info.target_problem_id)),
				contest_problem_id);
		}
	}

	// Transfer problem tags
	auto stmt = mysql.prepare("UPDATE IGNORE problem_tags SET problem_id=? WHERE problem_id=?");
	stmt.bindAndExecute(info.target_problem_id, problem_id);
	// Duplicates will not be updated or removed, so we need to delete them
	stmt = mysql.prepare("DELETE FROM problem_tags WHERE problem_id=?");
	stmt.bindAndExecute(problem_id);

	// Finally, delete the problem
	stmt = mysql.prepare("DELETE FROM problems WHERE id=?");
	stmt.bindAndExecute(problem_id);

	// Delete problem's files
	(void)remove_r(concat("problems/", problem_id));
	(void)remove(concat("problems/", problem_id, ".zip"));

	stmt = mysql.prepare("UPDATE jobs SET status=" JSTATUS_DONE_STR
		", data=? WHERE id=?");
	stmt.bindAndExecute(job_log.str, job_id);
}
