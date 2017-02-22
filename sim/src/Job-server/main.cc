#include "judge.h"
#include "problem.h"

#include <sim/constants.h>
#include <sim/mysql.h>
#include <sim/sqlite.h>
#include <simlib/process.h>
#include <simlib/time.h>
#include <sys/inotify.h>

using std::string;

MySQL::Connection db_conn;
SQLite::Connection sqlite_db;

static void processJobQueue() {

	// While job queue is not empty
	for (;;) try {
		MySQL::Result res = db_conn.executeQuery(
			"SELECT id, type, aux_id, info, creator, added FROM job_queue"
			" WHERE status=" JQSTATUS_PENDING_STR " AND type!=" JQTYPE_VOID_STR
			" ORDER BY priority DESC, id LIMIT 1");

		if (!res.next())
			break;

		string job_id = res[1];
		JobQueueType type = JobQueueType(res.getUInt(2));
		string aux_id = (res.isNull(3) ? "" : res[3]);
		string info = res[4];

		// Change the job status to IN_PROGRESS
		db_conn.executeUpdate("UPDATE job_queue"
			" SET status=" JQSTATUS_IN_PROGRESS_STR " WHERE id=" + job_id);

		// Choose action depending on the job type
		switch (type) {
		case JobQueueType::JUDGE_SUBMISSION:
			judgeSubmission(job_id, aux_id, res[6]);
			break;

		case JobQueueType::ADD_PROBLEM:
			addProblem(job_id, res[5], info);
			break;

		case JobQueueType::REUPLOAD_PROBLEM:
			reuploadProblem(job_id, res[5], info, aux_id);
			break;

		case JobQueueType::ADD_JUDGE_MODEL_SOLUTION:
			judgeModelSolution(job_id, JobQueueType::ADD_PROBLEM);
			break;

		case JobQueueType::REUPLOAD_JUDGE_MODEL_SOLUTION:
			judgeModelSolution(job_id, JobQueueType::REUPLOAD_PROBLEM);
			break;

		case JobQueueType::EDIT_PROBLEM:
		case JobQueueType::DELETE_PROBLEM:
			db_conn.executeUpdate("UPDATE job_queue"
				" SET status=" JQSTATUS_CANCELED_STR " WHERE id=" + job_id);
			break;

		case JobQueueType::VOID:
			break;

		}

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		// Give up for a couple of seconds to not litter the error log
		usleep(3e6);

	} catch (...) {
		ERRLOG_CATCH();
		// Give up for a couple of seconds to not litter the error log
		usleep(3e6);
	}
}

// Clean up database
static void cleanUpDB() {
	try {
		// Fix jobs that are in progress after the job-server died
		db_conn.executeUpdate("UPDATE job_queue"
			" SET status=" JQSTATUS_PENDING_STR
			" WHERE status=" JQSTATUS_IN_PROGRESS_STR);

		// Remove void (invalid) jobs and submissions that are older than 24 h
		auto yesterday_date = date("%Y-%m-%d %H:%M:%S",
			time(nullptr) - 24 * 60 * 60);
		db_conn.executeUpdate("DELETE FROM job_queue WHERE type=" JQTYPE_VOID_STR
			" AND added<'" + yesterday_date + "'");
		db_conn.executeUpdate("DELETE FROM submissions WHERE type=" STYPE_VOID_STR
			" AND submit_time<'" + yesterday_date + "'");

		// Remove void (invalid) problems and associated with them submissions
		// and jobs
		for (;;) { // TODO test it
			MySQL::Result res {db_conn.executeQuery("SELECT id FROM problems"
				" WHERE type=" PTYPE_VOID_STR " LIMIT 32")};
			if (res.rowCount() == 0)
				break;

			while (res.next()) {
				string pid = res[1];
				// Delete submissions
				db_conn.executeUpdate(
					"DELETE FROM submissions WHERE problem_id=" + pid);

				// Delete problem's files
				remove_r(StringBuff<PATH_MAX>{"problems/", pid});
				remove_r(StringBuff<PATH_MAX>{"problems/", pid, ".zip"});

				// Delete the problem (we have to do it here to prevent this
				// loop from going infinite)
				db_conn.executeUpdate("DELETE FROM problems WHERE id=" + pid);
			}
		}


	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
	}
}

int main() {
	// Change directory to process executable directory
	string cwd;
	try {
		cwd = chdirToExecDir();
	} catch (const std::exception& e) {
		errlog("Failed to change working directory: ", e.what());
	}

	// Loggers
	// stdlog like everything writes to stderr, so redirect stdout and stderr to
	// the log file
	if (freopen(JOB_SERVER_LOG, "a", stdout) == nullptr ||
		freopen(JOB_SERVER_LOG, "a", stderr) == nullptr)
	{
		errlog("Failed to open `", JOB_SERVER_LOG, '`', error(errno));
	}

	try {
		errlog.open(JOB_SERVER_ERROR_LOG);
	} catch (const std::exception& e) {
		errlog("Failed to open `", JOB_SERVER_ERROR_LOG, "`: ", e.what());
	}

	stdlog("Job server launch:\n"
		"PID: ", toStr(getpid()));

	// Install signal handlers
	struct sigaction sa;
	memset (&sa, 0, sizeof(sa));
	sa.sa_handler = &exit;

	(void)sigaction(SIGINT, &sa, nullptr);
	(void)sigaction(SIGQUIT, &sa, nullptr);
	(void)sigaction(SIGTERM, &sa, nullptr);

	// Connect to the databases
	try {
		sqlite_db = SQLite::Connection(SQLITE_DB_FILE, SQLITE_OPEN_READWRITE);
		db_conn = MySQL::createConnectionUsingPassFile(".db.config");

		cleanUpDB();

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return 1;
	}

	constexpr int OLD_WATCH_METHOD_SLEEP = 1e6; // 1 s

	// Initialize inotify
	int inotify_fd, wd;
	while ((inotify_fd = inotify_init()) == -1) {
		errlog("Error: inotify_init()", error(errno));
		// Run tests
		processJobQueue();
		usleep(OLD_WATCH_METHOD_SLEEP); // sleep
	}

	// If JOB_SERVER_NOTIFYING_FILE file does not exist create it
	if (access(JOB_SERVER_NOTIFYING_FILE, F_OK) == -1)
		(void)createFile(JOB_SERVER_NOTIFYING_FILE, S_IRUSR);

	auto startWatchingWd = [&] {
		while ((wd = inotify_add_watch(inotify_fd, JOB_SERVER_NOTIFYING_FILE,
			IN_ATTRIB | IN_MOVE_SELF)) == -1)
		{
			errlog("Error: inotify_add_watch()", error(errno));
			// Run tests
			processJobQueue();
			usleep(OLD_WATCH_METHOD_SLEEP); // sleep

			if (access(JOB_SERVER_NOTIFYING_FILE, F_OK) == -1)
				(void)createFile(JOB_SERVER_NOTIFYING_FILE, S_IRUSR);
		}
	};

	startWatchingWd();

	// Inotify buffer
	ssize_t len;
	char inotify_buff[sizeof(inotify_event) + NAME_MAX + 1];

	// Run tests before waiting for notification
	processJobQueue();

	// Wait for notification
	for (;;) {
		len = read(inotify_fd, inotify_buff, sizeof(inotify_buff));
		if (len < 1) {
			errlog("Error: read()", error(errno));
			continue;
		}

		struct inotify_event *event = (struct inotify_event *) inotify_buff;
		// If notify file has been moved
		if (event->mask & IN_MOVE_SELF) {
			(void)createFile(JOB_SERVER_NOTIFYING_FILE, S_IRUSR);
			inotify_rm_watch(inotify_fd, wd);
			startWatchingWd();

		// If notify file has disappeared
		} else if (event->mask & IN_IGNORED) {
			(void)createFile(JOB_SERVER_NOTIFYING_FILE, S_IRUSR);
			startWatchingWd();
		}

		// Run tests
		processJobQueue();
	}
	return 0;
}
