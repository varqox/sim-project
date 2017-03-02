#include <sim/constants.h>
#include <sim/jobs.h>
#include <sim/mysql.h>
#include <sim/sqlite.h>
#include <simlib/filesystem.h>
#include <simlib/process.h>
#include <simlib/sim/conver.h>
#include <simlib/spawner.h>
#include <simlib/time.h>
#include <simlib/utilities.h>
#include <utime.h>
#include <vector>

using jobs::AddProblemInfo;
using std::pair;
using std::string;
using std::vector;

extern MySQL::Connection db_conn;
extern SQLite::Connection sqlite_db;

namespace {

struct AddTrait {
	static constexpr JobQueueType JUDGE_MODEL_SOLUTION =
		JobQueueType::ADD_JUDGE_MODEL_SOLUTION;
};

struct ReuploadTrait {
	static constexpr JobQueueType JUDGE_MODEL_SOLUTION =
		JobQueueType::REUPLOAD_JUDGE_MODEL_SOLUTION;
};

} // anonymous namespace

// Before judging the model solution
template<class Trait>
static void firstStage(const string& job_id, AddProblemInfo info) {
	StringBuff<PATH_MAX> package_dest {"jobs_files/", job_id};
	StringBuff<PATH_MAX> tmp_package_path {package_dest, ".tmp/"};

	DirectoryRemover package_remover {package_dest};
	DirectoryRemover tmp_package_remover {tmp_package_path};

	sim::Conver::ReportBuff report;
	report.append("Stage: FIRST");

	auto set_failure = [&] {
		MySQL::Statement stmt = db_conn.prepare("UPDATE job_queue"
			" SET status=" JQSTATUS_FAILED_STR ", data=?"
			" WHERE id=? AND status!=" JQSTATUS_CANCELED_STR);
		stmt.setString(1, report.str);
		stmt.setString(2, job_id);
		stmt.executeUpdate();

		stdlog("Job: ", job_id, '\n', report.str);
	};

	/* Extract the package */

	// Remove old files/directories if exist
	(void)remove_r(package_dest);
	(void)remove_r(tmp_package_path);

	vector<string> zip_args;
	back_insert(zip_args, "unzip", "-o", concat(package_dest, ".zip"), "-d",
		tmp_package_path.str);

	report.append("Unpacking package...");

	// Run unzip
	{
		FileDescriptor zip_output {openUnlinkedTmpFile()};
		// It isn't a fatal error if zip_output is invalid, so it can be ignored
		Spawner::ExitStat es = Spawner::run(zip_args[0], zip_args,
			{-1, zip_output, zip_output, 500'000'000 /* 500 s */, 512 << 20});

		// Append unzip's output to the report
		(void)lseek(zip_output, 0, SEEK_SET);
		report.append(getFileContents(zip_output));

		if (es.code) {
			report.append("Unpacking error: ", es.message);
			return set_failure();
		}

		report.append("Unpacking took ", usecToSecStr(es.runtime, 3), "s.");
	}


	// Check if the package has the master directory
	int entities = 0;
	StringBuff<PATH_MAX> master_dir;
	forEachDirComponent(tmp_package_path, [&](const dirent* file) {
		++entities;
		if (master_dir.size())
			return;

		if (file->d_type == DT_DIR ||
			(file->d_type == DT_UNKNOWN && isDirectory(
				StringBuff<PATH_MAX>{tmp_package_path, file->d_name})))
		{
			master_dir = file->d_name;
		}
	});

	if (master_dir.len && entities == 1)
		tmp_package_path.append(master_dir);

	// Move package to the final destination
	if (rename(tmp_package_path.str, package_dest.str) != 0) {
		errlog(__FILE__ ":", toStr(__LINE__), ": rename(`", tmp_package_path,
			"`, `", package_dest, '`');
		report.append("Error: rename(`", tmp_package_path, "`, `", package_dest,
			'`');
		return set_failure();
	}


	// Remove the .tmp directory
	if (master_dir.len) {
		// Truncate master_dir from the path
		tmp_package_path[tmp_package_path.len -= master_dir.len] = '\0';
		(void)tmp_package_remover.removeTarget();
	}

	/* Construct Simfile */

	sim::Conver conver;
	conver.setVerbosity(true);
	conver.setPackagePath(package_dest.str);

	// Set Conver options
	sim::Conver::Options opts;
	opts.name = info.name;
	opts.label = info.label;
	opts.memory_limit = info.memory_limit;
	opts.global_time_limit = info.global_time_limit;
	opts.ignore_simfile = info.ignore_simfile;
	opts.force_auto_time_limits_setting = info.force_auto_limit;
	opts.compilation_time_limit = SOLUTION_COMPILATION_TIME_LIMIT;
	opts.compilation_errors_max_length =
		COMPILATION_ERRORS_MAX_LENGTH;
	opts.proot_path = PROOT_PATH;

	std::pair<sim::Conver::Status, sim::Simfile> p;
	try {
		p = conver.constructSimfile(opts);
	} catch (const std::exception& e) {
		report.append(conver.getReport(), "Conver failed: ", e.what());
		return set_failure();
	}

	// Check problem's name's length
	if (p.second.name.size() > PROBLEM_NAME_MAX_LEN) {
		report.append("Problem's name is too long (max allowed length: ",
			toStr(PROBLEM_NAME_MAX_LEN), ')');
		return set_failure();
	}

	// Check problem's label's length
	if (p.second.label.size() > PROBLEM_LABEL_MAX_LEN) {
		report.append("Problem's label is too long (max allowed length: ",
			toStr(PROBLEM_LABEL_MAX_LEN), ')');
		return set_failure();
	}

	report.append(conver.getReport());

	// Put the Simfile into the package
	putFileContents(package_dest.append("/Simfile"), p.second.dump());

	info.stage = AddProblemInfo::SECOND; // Advance the stage

	switch (p.first) {
	case sim::Conver::Status::COMPLETE: {
		// Advance the job to the SECOND stage
		MySQL::Statement stmt = db_conn.prepare("UPDATE job_queue"
			" SET status=" JQSTATUS_PENDING_STR ", info=?, data=?"
			" WHERE id=? AND status!=" JQSTATUS_CANCELED_STR);
		stmt.setString(1, info.dump());
		stmt.setString(2, report.str);
		stmt.setString(3, job_id);
		stmt.executeUpdate();
		break;
	}
	case sim::Conver::Status::NEED_MODEL_SOLUTION_JUDGE_REPORT: {
		// Transform the job into a JUDGE_MODEL_SOLUTION job
		MySQL::Statement stmt = db_conn.prepare("UPDATE job_queue"
			" SET type=?, status=" JQSTATUS_PENDING_STR ", info=?, data=?"
			" WHERE id=? AND status!=" JQSTATUS_CANCELED_STR);
		stmt.setUInt(1, uint(Trait::JUDGE_MODEL_SOLUTION));
		stmt.setString(2, info.dump());
		stmt.setString(3, report.str);
		stmt.setString(4, job_id);
		stmt.executeUpdate();
		break;
	}
	}

	stdlog("Job: ", job_id, '\n', report.str);
	package_remover.cancel();
}

/**
 * @brief Continue adding the problem after judging the model solution
 * @details It leaves add problem and its submissions with type set to VOID
 *
 * @param job_id job id
 * @param job_owner job owner
 * @param info add info
 * @param report report to append the log to
 * @tparam Trait trait to use
 *
 * @return Id of the added problem
 *
 * @errors If any error occurs an appropriate exception is thrown
 */
template<class Trait>
static string secondStage(const string& job_id, const string& job_owner,
	AddProblemInfo info, sim::Conver::ReportBuff& report)
{
	DirectoryRemover job_package_remover
		{StringBuff<PATH_MAX>{"jobs_files/", job_id}};

	report.append("\nStage: SECOND");

	// Load the Simfile
	string simfile_str {getFileContents(
		StringBuff<PATH_MAX>{"jobs_files/", job_id, "/Simfile"})};

	sim::Simfile sf {simfile_str};
	sf.loadName();
	sf.loadLabel();
	sf.loadSolutions();

	// Add the problem to the database
	string current_date = date();
	MySQL::Statement stmt {db_conn.prepare(
		"INSERT problems (type, name, label, simfile, owner, added,"
			" last_edit)"
		" VALUES(" PTYPE_VOID_STR ", ?, ?, ?, ?, ?, ?)")};
	stmt.setString(1, sf.name);
	stmt.setString(2, sf.label);
	stmt.setString(3, std::move(simfile_str));
	stmt.setString(4, job_owner);
	stmt.setString(5, current_date);
	stmt.setString(6, current_date);
	stmt.executeUpdate();

	// Obtain the new problem's id
	string problem_id = db_conn.lastInsertId();

	/* Begin transaction */

	// SQLite transaction is handled in the function addProblem() that calls
	// this function so we don't have to worry about that
	auto rollbacker = [&] {
		SignalBlocker sb; // Prevent the transaction rollback from interruption

		// Delete problem's files
		(void)remove_r(StringBuff<PATH_MAX>{"problems/", problem_id});
		(void)remove(StringBuff<PATH_MAX>{"problems/", problem_id, ".zip"});

		// Delete the problem and its submissions
		db_conn.executeUpdate("DELETE FROM problems WHERE id=" +
			problem_id);
		db_conn.executeUpdate("DELETE FROM submissions WHERE problem_id=" +
			problem_id);
	};
	CallInDtor<decltype(rollbacker)> rollback_maker {rollbacker};

	// Insert the problem into the SQLite FTS5 table `problems`
	SQLite::Statement sqlite_stmt {sqlite_db.prepare(
		"INSERT INTO problems (rowid, type, name, label)"
		" VALUES(?, ?, ?, ?)")};
	sqlite_stmt.bindText(1, problem_id, SQLITE_STATIC);
	sqlite_stmt.bindInt(2, int(info.public_problem ?
		ProblemType::PUBLIC : ProblemType::PRIVATE));
	sqlite_stmt.bindText(3, sf.name, SQLITE_STATIC);
	sqlite_stmt.bindText(4, sf.label, SQLITE_STATIC);
	throw_assert(sqlite_stmt.step() == SQLITE_DONE);

	// Move package's directory to its destination
	StringBuff<PATH_MAX> package_dest {"problems/", problem_id};
	if (move(StringBuff<PATH_MAX>{"jobs_files/", job_id}, package_dest))
		THROW("move()", error(errno));

	job_package_remover.cancel();

	// Zip the package
	report.append("Zipping the package...");
	{
		FileDescriptor zip_output {openUnlinkedTmpFile()};
		// It isn't a fatal error if zip_output is invalid, so it can be ignored
		vector<string> zip_args;
		back_insert(zip_args, "zip", "-rq", concat(problem_id, ".zip"),
			problem_id);
		Spawner::ExitStat es = Spawner::run(zip_args[0], zip_args,
			{-1, zip_output, zip_output, 0, 512 << 20}, "problems");

		// Append zip's output to the the report
		(void)lseek(zip_output, 0, SEEK_SET);
		report.append(getFileContents(zip_output));

		if (es.code)
			THROW("Zip error: ", es.message);

		report.append("Zipping took ", usecToSecStr(es.runtime, 3), "s.");
	}

	// Submit the solutions
	report.append("Submitting solutions...");
	package_dest.append('/');
	for (auto&& solution : sf.solutions) {
		report.append("Submit: ", solution);

		current_date = date();
		stmt = db_conn.prepare("INSERT submissions (user_id, problem_id,"
				" round_id, parent_round_id, contest_round_id, type,"
				" status, submit_time, last_judgment, initial_report,"
				" final_report)"
			" VALUES(" SIM_ROOT_UID ", ?, NULL, NULL, NULL, "
				STYPE_VOID_STR ", " SSTATUS_PENDING_STR ", ?, ?, '', '')");
		stmt.setString(1, problem_id);
		stmt.setString(2, current_date);
		stmt.setString(3, date("%Y-%m-%d %H:%M:%S", 0));
		stmt.executeUpdate();

		string submission_id = db_conn.lastInsertId();

		// Make solution's source code the submission's source code
		if (copy(StringBuff<PATH_MAX * 2>{package_dest, solution},
			StringBuff<PATH_MAX>{"solutions/", submission_id, ".cpp"}))
		{
			THROW("Copying solution `", solution, "`: copy()",
				error(errno));
		}

		// Create a job to judge the submission
		stmt = db_conn.prepare("INSERT job_queue (creator, status,"
				" priority, type, added, aux_id, info, data)"
			" VALUES(NULL, " JQSTATUS_PENDING_STR ", ?, "
				JQTYPE_JUDGE_SUBMISSION_STR ", ?, ?, ?, '')");
		// Problem's solutions are more important than ordinary submissions
		stmt.setUInt(1, priority(JobQueueType::JUDGE_SUBMISSION) + 1);
		stmt.setString(2, current_date);
		stmt.setString(3, submission_id);
		stmt.setString(4, jobs::dumpString(problem_id));
		stmt.executeUpdate();
	}
	report.append("Done.");

	// Notify the job-server that there are submissions to judge
	utime(JOB_SERVER_NOTIFYING_FILE, nullptr);

	// TODO: fix updating submission status so that it will work with PROBLEM_SOLUTION submissions and other queries connected to this problem

	// End transaction
	rollback_maker.cancel();

	return problem_id;
}

/**
 * @brief Adds problem to the problemset, submits its solutions and calls
 *   @p func. Note that this function does not make the problem visible via
 *   the web-interface.
 *
 * @param job_id job id
 * @param job_owner job owner
 * @param aux_id job's auxiliary id
 * @param info problem info
 * @param func function to call just after finishing adding the problem
 * @tparam Trait trait to use
*/
template<class Trait, class Func>
static void addProblem(const string& job_id, const string& job_owner,
	const string& aux_id, AddProblemInfo info, Func&& func)
{
	switch (info.stage) {
	case AddProblemInfo::FIRST:
		firstStage<Trait>(job_id, std::move(info));
		break;

	case AddProblemInfo::SECOND: {
		sim::Conver::ReportBuff report;
		MySQL::Statement stmt = db_conn.prepare("UPDATE job_queue"
			" SET status=?, aux_id=?, data=CONCAT(data,?) WHERE id=?");
		try {
			sqlite_db.execute("BEGIN");
			// Added problem's id
			string apid =
				secondStage<Trait>(job_id, job_owner, std::move(info), report);
			func(apid, report);
			sqlite_db.execute("COMMIT");

			stmt.setUInt(1, uint(JobQueueStatus::DONE));
			stmt.setString(2, std::is_same<Trait, AddTrait>::value ?
				apid : aux_id);

		} catch (const std::exception& e) {
			// To avoid exceptions C API is used
			sqlite3_exec(sqlite_db, "ROLLBACK", nullptr, nullptr, nullptr);

			ERRLOG_CATCH(e);
			report.append("Caught exception: ", e.what());

			stmt.setUInt(1, uint(JobQueueStatus::FAILED));
			if (std::is_same<Trait, ReuploadTrait>::value)
				stmt.setString(2, aux_id);
			else
				stmt.setNull(2);
		}

		stmt.setString(3, report.str);
		stmt.setString(4, job_id);
		stmt.executeUpdate();

		stdlog("Job: ", job_id, '\n', report.str);
		break;
	}
	}
}


void addProblem(const string& job_id, const string& job_owner, StringView info)
{
	AddProblemInfo p_info {info};
	addProblem<AddTrait>(job_id, job_owner, "", p_info,
		[&](string problem_id, sim::Conver::ReportBuff&) {
			// Make the problem and its solutions' submissions public
			MySQL::Statement stmt = db_conn.prepare(
				"UPDATE problems p, submissions s"
				" SET p.type=?, s.type=" STYPE_PROBLEM_SOLUTION_STR
				" WHERE p.id=? AND s.problem_id=?");
			stmt.setUInt(1, uint(p_info.public_problem ?
				ProblemType::PUBLIC : ProblemType::PRIVATE));
			stmt.setString(2, problem_id);
			stmt.setString(3, problem_id);
			stmt.executeUpdate();
		});

}

void reuploadProblem(const string& job_id, const string& job_owner,
	StringView info, const std::string& aux_id)
{
	AddProblemInfo p_info {info};
	addProblem<ReuploadTrait>(job_id, job_owner, aux_id, p_info,
		[&](string problem_id, sim::Conver::ReportBuff& report) {
			// TODO: the problem is invalid for a while - do something so that
			//   judging worker won't use it during that time
			report.append("Replacing the old problem with the new one...");

			// Make a backup of the problem
			vector<pair<bool, string>> pbackup;
			MySQL::Result res {db_conn.executeQuery("SELECT * FROM problems"
				" WHERE id=" + aux_id)};
			string prestore_sql;
			if (res.next()) {
				prestore_sql = "REPLACE problems VALUES(";

				uint n = res.impl()->getMetaData()->getColumnCount();
				pbackup.resize(n);
				for (uint i = 0; i < n; ++i) {
					pbackup[i] = {res.isNull(i + 1), res[i + 1]};
					prestore_sql += "?,";
				}

				prestore_sql.back() = ')';
			}

			// Make a backup of the problem's solutions' submissions
			vector<vector<pair<bool, string>>> sbackup;
			string srestore_sql;
			res = db_conn.executeQuery("SELECT * FROM submissions"
				" WHERE type=" STYPE_PROBLEM_SOLUTION_STR " AND problem_id=" +
				aux_id);
			if (res.next()) {
				uint n = res.impl()->getMetaData()->getColumnCount();
				// Restore sql
				srestore_sql = "REPLACE submissions VALUES(";
				for (uint i = 0; i < n; ++i)
					srestore_sql += "?,";
				srestore_sql.back() = ')';

				// Backup the submissions
				do {
					sbackup.emplace_back();
					sbackup.back().resize(n);
					for (uint i = 0; i < n; ++i)
						sbackup.back()[i] = {res.isNull(i + 1), res[i + 1]};

				} while (res.next());
			}

			StringBuff<PATH_MAX> package_path {"problems/", aux_id};
			StringBuff<PATH_MAX> package_zip {package_path, ".zip"};
			StringBuff<PATH_MAX> backuped_package {package_path, ".backup"};
			StringBuff<PATH_MAX> backuped_zip {package_zip, ".backup"};

			(void)remove_r(backuped_package);
			(void)remove_r(backuped_zip);

			ThreadSignalBlocker sb; // This part cannot be interrupted
			try {
				// Backup old problem's files
				if (rename(package_path.str, backuped_package.str)) {
					package_path = package_zip = {}; // Prevent restoring of
					                                 // these
					THROW("rename()", error(errno));
				}

				if (rename(package_zip.str, backuped_zip.str)) {
					package_zip = {}; // Prevent restoring of this
					THROW("rename()", error(errno));
				}

				// Delete the old problem
				db_conn.executeUpdate("DELETE FROM problems WHERE id=" +
					aux_id);

				// Replace the old problem with the new one
				MySQL::Statement stmt {db_conn.prepare(
					"UPDATE problems SET id=?, type=? WHERE id=?")};
				stmt.setString(1, aux_id);
				stmt.setUInt(2, uint(p_info.public_problem ?
					ProblemType::PUBLIC : ProblemType::PRIVATE));
				stmt.setString(3, problem_id);
				stmt.executeUpdate();

				// The same with files
				StringBuff<PATH_MAX> ppath {"problems/", problem_id};
				StringBuff<PATH_MAX> pzip {ppath, ".zip"};
				if (rename(ppath.str, package_path.str) ||
					rename(pzip.str, package_zip.str))
				{
					THROW("rename()", error(errno));
				}

				// Replace the problem in the SQLite FTS5 table `problems`
				SQLite::Statement sqlite_stmt {sqlite_db.prepare(
					"UPDATE OR REPLACE problems SET rowid=? WHERE rowid=?")};
				sqlite_stmt.bindText(1, aux_id, SQLITE_STATIC);
				sqlite_stmt.bindText(2, problem_id, SQLITE_STATIC);
				throw_assert(sqlite_stmt.step() == SQLITE_DONE);

				// Delete the old problem's solutions' submissions
				db_conn.executeUpdate("DELETE FROM submissions WHERE type="
					STYPE_PROBLEM_SOLUTION_STR " AND problem_id=" + aux_id);

				// Activate the new problem's solutions' submissions
				stmt = db_conn.prepare("UPDATE submissions"
					" SET problem_id=?, type=" STYPE_PROBLEM_SOLUTION_STR
					" WHERE problem_id=?");
				stmt.setString(1, aux_id);
				stmt.setString(2, problem_id);
				stmt.executeUpdate();

				(void)remove_r(backuped_package);
				(void)remove_r(backuped_zip);

			} catch (const std::exception& e) {
				ERRLOG_FORWARDING(e);

				// Restore problem's files
				(void)rename(backuped_package.str, package_path.str);
				(void)rename(backuped_zip.str, package_zip.str);

				// Restore the problem in the database
				if (prestore_sql.size()) {
					MySQL::Statement stmt {db_conn.prepare(prestore_sql)};
					for (uint i = 0; i < pbackup.size(); ++i) {
						if (pbackup[i].first)
							stmt.setNull(i + 1);
						else
							stmt.setString(i + 1, pbackup[i].second);
					}
					stmt.executeUpdate();
				}

				// Restore problem's solutions' submissions in the database
				if (srestore_sql.size()) {
					MySQL::Statement stmt = db_conn.prepare(srestore_sql);
					for (auto&& v : sbackup) {
						for (uint i = 0; i < v.size(); ++i) {
							if (v[i].first)
								stmt.setNull(i + 1);
							else
								stmt.setString(i + 1, v[i].second);
						}
						stmt.executeUpdate();
					}
				}

				throw;
			}
		});
}
