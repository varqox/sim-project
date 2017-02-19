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
using std::string;
using std::vector;

extern MySQL::Connection db_conn;
extern SQLite::Connection sqlite_db;

// Before judging the model solution
static void firstStage(const string& job_id, AddProblemInfo info,
	bool reupload_problem)
{
	sim::Conver::ReportBuff report;
	report.append("Stage: FIRST");

	auto set_failure= [&] {
		MySQL::Statement stmt = db_conn.prepare("UPDATE job_queue"
			" SET status=" JQSTATUS_FAILED_STR ", data=?"
			" WHERE id=? AND status!=" JQSTATUS_CANCELED_STR);
		stmt.setString(1, report.str);
		stmt.setString(2, job_id);
		stmt.executeUpdate();

		stdlog("Job: ", job_id, '\n', report.str);
	};

	/* Extract package */

	// Remove old files/directories if exist
	StringBuff<PATH_MAX> package_dest {"jobs_files/", job_id};
	StringBuff<PATH_MAX> tmp_package_path {package_dest, ".tmp/"};

	remove_r(package_dest);
	remove_r(tmp_package_path);

	vector<string> zip_args;
	back_insert(zip_args, "unzip", "-o", concat(package_dest, ".zip"), "-d",
		tmp_package_path.str);

	report.append("Unpacking package...");

	// Run unzip
	{
		FileDescriptor zip_output {openUnlinkedTmpFile()};
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
		remove_r(tmp_package_path);
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
		report.str += conver.getReport();
		report.append("Conver failed: ", e.what());
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

	report.str += conver.getReport();

	// Put the Simfile in the package
	putFileContents(package_dest.append("/Simfile"), p.second.dump());

	info.stage = AddProblemInfo::SECOND; // Set stage to SECOND

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
		stmt.setUInt(1, uint(reupload_problem ?
			JobQueueType::REUPLOAD_JUDGE_MODEL_SOLUTION
			: JobQueueType::ADD_JUDGE_MODEL_SOLUTION));
		stmt.setString(2, info.dump());
		stmt.setString(3, report.str);
		stmt.setString(4, job_id);
		stmt.executeUpdate();
		break;
	}
	}

	stdlog("Job: ", job_id, '\n', report.str);
}

// After judging the model solution
static void secondStage(const string& job_id, const string& job_owner,
	AddProblemInfo info)
{
	sim::Conver::ReportBuff report;
	report.append("Stage: SECOND");

	try {
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
			" VALUES(?, ?, ?, ?, ?, ?, ?)")};
		stmt.setUInt(1, int(info.public_problem ?
			ProblemType::PUBLIC : ProblemType::PRIVATE));
		stmt.setString(2, sf.name);
		stmt.setString(3, sf.label);
		stmt.setString(4, std::move(simfile_str));
		stmt.setString(5, job_owner);
		stmt.setString(6, current_date);
		stmt.setString(7, current_date);
		stmt.executeUpdate();

		// Obtain new problem's id
		string problem_id = db_conn.lastInsertId();

		// Begin transaction
		sqlite_db.execute("BEGIN");
		auto rollbacker = [&] {
			SignalBlocker sb; // Prevent the transaction from interrupting
			sqlite_db.execute("ROLLBACK"); // SQLite
			// Remove the problem and its submissions
			db_conn.executeUpdate("DELETE FROM problems WHERE id=" +
				problem_id);
			db_conn.executeUpdate("DELETE FROM submissions WHERE problem_id=" +
				problem_id);
			// Files
			remove_r(StringBuff<PATH_MAX>{"problems/", problem_id});
			remove_r(StringBuff<PATH_MAX>{"problems/", problem_id, ".zip"});
		};
		CallInDtor<decltype(rollbacker)> rollback_maker {rollbacker};

		// Insert the problem into the SQLite FTS5
		SQLite::Statement sqlite_stmt {sqlite_db.prepare(
			"INSERT INTO problems (rowid, type, name, label)"
			" VALUES(?, ?, ?, ?)")};
		sqlite_stmt.bindText(1, problem_id, SQLITE_STATIC);
		sqlite_stmt.bindInt(2, int(info.public_problem ?
			ProblemType::PUBLIC : ProblemType::PRIVATE));
		sqlite_stmt.bindText(3, sf.name, SQLITE_STATIC);
		sqlite_stmt.bindText(4, sf.label, SQLITE_STATIC);
		throw_assert(sqlite_stmt.step() == SQLITE_DONE);

		// Move package directory to its destination
		StringBuff<PATH_MAX> package_dest {"problems/", problem_id};
		if (move(StringBuff<PATH_MAX>{"jobs_files/", job_id}, package_dest))
			THROW("move()", error(errno));

		// Zip the package
		report.append("Zipping the package...");
		{
			FileDescriptor zip_output {openUnlinkedTmpFile()};
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
			// Begin transaction
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

			// Enable the submission
			db_conn.executeUpdate("UPDATE submissions"
				" SET type=" STYPE_PROBLEM_SOLUTION_STR
				" WHERE id=" + submission_id);

			// Create a job to judge the submission
			stmt = db_conn.prepare("INSERT job_queue (creator, status,"
					" priority, type, added, aux_id, info, data)"
				" VALUES(" SIM_ROOT_UID ", " JQSTATUS_PENDING_STR ", ?, "
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

		// TODO: sumission.last_jugment - update it during judgment
		// TODO: fix updating submission status so that it will work with PROBLEM_SOLUTION submissions and other queries connected to this problem

		stmt = db_conn.prepare("UPDATE job_queue"
			" SET status=" JQSTATUS_DONE_STR ", aux_id=?, data=CONCAT(data,?)"
			" WHERE status!=" JQSTATUS_CANCELED_STR " AND id=?");
		stmt.setString(1, problem_id);
		stmt.setString(2, report.str);
		stmt.setString(3, job_id);

		// End transaction
		if (stmt.executeUpdate() == 1) {
			sqlite_db.execute("COMMIT");
			rollback_maker.cancel();
		}

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		report.append(e.what());

		// Set failure
		MySQL::Statement stmt = db_conn.prepare("UPDATE job_queue"
			" SET status=" JQSTATUS_FAILED_STR ", data=CONCAT(data,?)"
			" WHERE id=? AND status!=" JQSTATUS_CANCELED_STR);
		stmt.setString(1, report.str);
		stmt.setString(2, job_id);
		stmt.executeUpdate();
	}

	stdlog("Job: ", job_id, '\n', report.str);
}

void addOrReuploadProblem(const string& job_id, const string& job_owner,
	StringView info, const string& aux_id, bool reupload_problem)
{
	AddProblemInfo p_info {info};
	if (p_info.stage == AddProblemInfo::FIRST)
		firstStage(job_id, std::move(p_info), reupload_problem);
	else
		secondStage(job_id, job_owner, std::move(p_info));
}
