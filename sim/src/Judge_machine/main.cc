#include "judge.h"
#include <sim/constants.h>
#include <sim/db.h>
#include <simlib/debug.h>
#include <simlib/process.h>
#include <sys/inotify.h>

using std::string;

static constexpr int OLD_WATCH_METHOD_SLEEP = 1e6; // 1 s
static DB::Connection db_conn;
TemporaryDirectory tmp_dir;
unsigned VERBOSITY = 2; // 0 - quiet, 1 - normal, 2 or more - verbose

static void processSubmissionQueue() {
	// While submission queue is not empty
	for (;;) {
		try {
			// TODO: fix bug (rejudged submissions)
			DB::Result res = db_conn.executeQuery(
				"SELECT id, user_id, round_id, problem_id FROM submissions "
				"WHERE status='waiting' ORDER BY queued LIMIT 10");
			if (!res.next())
				return; // Queue is empty

			do {
				string submission_id = res[1];
				string user_id = res[2];
				string round_id = res[3];
				string problem_id = res[4];

				// Judge
				JudgeResult jres = judge(submission_id, problem_id);

				// Update submission
				DB::Statement stmt;
				if (jres.status == JudgeResult::COMPILE_ERROR ||
					jres.status == JudgeResult::JUDGE_ERROR)
				{
					stmt = db_conn.prepare(
						// x.id - id of submission which will have final=1
						//   (latest with status = 'ok' or 'error')
						// y.id - id of submissions which will have final=0
						//   UNION with 0 - because if x was empty then whole
						//   query wouldn't be executed (and 0 is safe because
						//   submission with id=0 does not exist)
						"UPDATE submissions s, "
							"((SELECT id FROM submissions WHERE user_id=? AND "
								"round_id=? AND (status='ok' OR status='error')"
								"AND id!=? ORDER BY id DESC LIMIT 1) "
							"UNION (SELECT 0 AS id)) x, "

							"((SELECT id FROM submissions WHERE user_id=? AND "
								"round_id=? AND final=1) "
							"UNION (SELECT 0 AS id)) y "
						// set final properly and other columns ONLY for just
						// judged submission
						"SET s.final=IF(s.id=x.id, 1, 0),"
							"s.status=IF(s.id=?, ?, s.status),"
							"s.initial_report=IF(s.id=?, ?, s.initial_report),"
							"s.final_report=IF(s.id=?, ?, s.final_report),"
							"s.score=IF(s.id=?, NULL, s.score)"
						"WHERE s.id=x.id OR s.id=y.id OR s.id=?");

					stmt.setString(7, (jres.status == JudgeResult::COMPILE_ERROR
						? "c_error" : "judge_error"));
					stmt.setString(13, submission_id);

				} else {
					stmt = db_conn.prepare(
						// x.id - id of submission which will have final=1
						//   (latest with status = 'ok' or 'error')
						// y.id - id of submissions which will have final=0
						//   UNION with 0 - because if x was empty then whole
						//   query wouldn't be executed (and 0 is safe because
						//   submission with id=0 does not exist)
						"UPDATE submissions s, "
							"((SELECT id FROM submissions WHERE user_id=? AND "
								"round_id=? AND (status='ok' OR status='error' "
								"OR id=?) ORDER BY id DESC LIMIT 1) "
							"UNION (SELECT 0 AS id)) x, "

							"((SELECT id FROM submissions WHERE user_id=? AND "
								"round_id=? AND final=1) "
							"UNION (SELECT 0 AS id)) y "
						// set final properly and other columns ONLY for just
						// judged submission
						"SET s.final=IF(s.id=x.id, 1, 0),"
							"s.status=IF(s.id=?, ?, s.status),"
							"s.initial_report=IF(s.id=?, ?, s.initial_report),"
							"s.final_report=IF(s.id=?, ?, s.final_report),"
							"s.score=IF(s.id=?, ?, s.score)"
						"WHERE s.id=x.id OR s.id=y.id OR s.id=?");

					stmt.setString(7, (jres.status == JudgeResult::OK
						? "ok" : "error"));
					stmt.setInt64(13, jres.score);
					stmt.setString(14, submission_id);
				}

				stmt.setString(1, user_id);
				stmt.setString(2, round_id);
				stmt.setString(3, submission_id);
				stmt.setString(4, user_id);
				stmt.setString(5, round_id);
				stmt.setString(6, submission_id);
				stmt.setString(8, submission_id);
				stmt.setString(9, jres.initial_report);
				stmt.setString(10, submission_id);
				stmt.setString(11, jres.final_report);
				stmt.setString(12, submission_id);

				stmt.executeUpdate();
			} while (res.next());

		} catch (const std::exception& e) {
			ERRLOG_CATCH(e);
			usleep(1e6); // Give up for a second to not litter the error log

		} catch (...) {
			ERRLOG_CATCH();
			usleep(1e6); // Give up for a second to not litter the error log
		}
	}
}

void startWatching(int inotify_fd, int& wd) {
	while ((wd = inotify_add_watch(inotify_fd, "judge-machine.notify",
		IN_ATTRIB | IN_MOVE_SELF)) == -1)
	{
		errlog("Error: inotify_add_watch()", error(errno));
		// Run tests
		processSubmissionQueue();
		usleep(OLD_WATCH_METHOD_SLEEP); // sleep

		if (access("judge-machine.notify", F_OK) == -1)
			(void)createFile("judge-machine.notify", S_IRUSR);
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
	try {
		stdlog.open(JUDGE_LOG);
	} catch (const std::exception& e) {
		errlog("Failed to open '", JUDGE_LOG, "': ", e.what());
	}

	try {
		errlog.open(JUDGE_ERROR_LOG);
	} catch (const std::exception& e) {
		errlog("Failed to open '", JUDGE_ERROR_LOG, "': ", e.what());
	}

	// Install signal handlers
	struct sigaction sa;
	memset (&sa, 0, sizeof(sa));
	sa.sa_handler = &exit;

	(void)sigaction(SIGINT, &sa, nullptr);
	(void)sigaction(SIGQUIT, &sa, nullptr);
	(void)sigaction(SIGTERM, &sa, nullptr);

	try {
		// Connect to database
		db_conn = DB::createConnectionUsingPassFile(".db.config");
		// Create tmp_dir
		tmp_dir = TemporaryDirectory("/tmp/sim-judge-machine.XXXXXX");

	} catch (const std::exception& e) {
		errlog("Caught exception: ", __FILE__, ':', toString(__LINE__), " -> ",
			e.what());
		return 1;
	}

	// Initialise inotify
	int inotify_fd, wd;
	while ((inotify_fd = inotify_init()) == -1) {
		errlog("Error: inotify_init()", error(errno));
		// Run tests
		processSubmissionQueue();
		usleep(OLD_WATCH_METHOD_SLEEP); // sleep
	}

	// If "judge-machine.notify" file does not exist create it
	if (access("judge-machine.notify", F_OK) == -1)
		(void)createFile("judge-machine.notify", S_IRUSR);

	startWatching(inotify_fd, wd);

	// Inotify buffer
	ssize_t len;
	char inotify_buff[sizeof(inotify_event) + NAME_MAX + 1];

	// Run tests before waiting for notification
	processSubmissionQueue();

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
			(void)createFile("judge-machine.notify", S_IRUSR);
			inotify_rm_watch(inotify_fd, wd);
			startWatching(inotify_fd, wd);

		// If notify file has disappeared
		} else if (event->mask & IN_IGNORED) {
			(void)createFile("judge-machine.notify", S_IRUSR);
			startWatching(inotify_fd, wd);
		}

		// Run tests
		processSubmissionQueue();
	}
	return 0;
}
