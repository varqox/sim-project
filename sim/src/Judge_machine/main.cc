#include "judge.h"
#include "main.h"

#include "../include/db.h"
#include "../simlib/include/logger.h"
#include "../simlib/include/process.h"

#include <cppconn/prepared_statement.h>
#include <csignal>
#include <limits.h>
#include <sys/inotify.h>

using std::string;

static const int OLD_WATCH_METHOD_SLEEP = 1 * 1000000; // 1s
static DB::Connection db_conn;
TemporaryDirectory tmp_dir;
unsigned VERBOSITY = 2; // 0 - quiet, 1 - normal, 2 or more - verbose

static void processSubmissionQueue() {
	try {
		UniquePtr<sql::Statement> stmt(db_conn->createStatement());

		// While submission queue is not empty
		for (;;) {
			// TODO: fix bug (rejudged submissions)
			UniquePtr<sql::ResultSet> res(stmt->executeQuery(
				"SELECT id, user_id, round_id, problem_id FROM submissions "
				"WHERE status='waiting' ORDER BY queued LIMIT 10"));
			if (!res->next())
				return; // Queue is empty

			do {
				string submission_id = res->getString(1);
				string user_id = res->getString(2);
				string round_id = res->getString(3);
				string problem_id = res->getString(4);

				// Judge
				JudgeResult jres = judge(submission_id, problem_id);

				// Update submission
				if (putFileContents(concat("submissions/", submission_id),
						jres.content) == -1)
					throw std::runtime_error("putFileContents(): -1");

				// Update final
				UniquePtr<sql::PreparedStatement> pstmt;
				if (jres.status == JudgeResult::COMPILE_ERROR ||
						jres.status == JudgeResult::JUDGE_ERROR) {
					pstmt.reset(db_conn->prepareStatement("UPDATE submissions "
						"SET final=false, status=?, score=? WHERE id=?"));
					pstmt->setString(1,
						(jres.status == JudgeResult::COMPILE_ERROR
							? "c_error" : "judge_error"));
					pstmt->setNull(2, 0);
					pstmt->setString(3, submission_id);

				} else {
					pstmt.reset(db_conn->prepareStatement(
						"UPDATE submissions s, "
							"((SELECT id FROM submissions "
									"WHERE user_id=? AND round_id=? "
										"AND final=1) "
								"UNION "
									"(SELECT ? AS id) "
								"LIMIT 1) x "
						"SET s.final=IF(s.id=?, "
								"IF(x.id<=s.id, 1, 0), "
								"IF(s.id>?, 1, 0)), "
							"s.status=IF(s.id=?, ?, s.status),"
							"s.score=IF(s.id=?, ?, s.score)"
						"WHERE s.id=x.id OR s.id=?"));
					pstmt->setString(1, user_id);
					pstmt->setString(2, round_id);
					pstmt->setString(3, submission_id);
					pstmt->setString(4, submission_id);
					pstmt->setString(5, submission_id);
					pstmt->setString(6, submission_id);
					pstmt->setString(7, (jres.status == JudgeResult::OK
						? "ok" : "error"));
					pstmt->setString(8, submission_id);
					pstmt->setInt64(9, jres.score);
					pstmt->setString(10, submission_id);
				}

				pstmt->executeUpdate();
			} while (res->next());
		}

	} catch (const std::exception& e) {
		error_log("Caught exception: ", __FILE__, ':', toString(__LINE__),
			" - ", e.what());

	} catch (...) {
		error_log("Caught exception: ", __FILE__, ':', toString(__LINE__));
	}
}

void startWatching(int inotify_fd, int& wd) {
	while ((wd = inotify_add_watch(inotify_fd, "judge-machine.notify",
			IN_ATTRIB)) == -1) {
		error_log("Error: inotify_add_watch()", error(errno));
		// Run tests
		processSubmissionQueue();
		usleep(OLD_WATCH_METHOD_SLEEP); // sleep

		if (access("judge-machine.notify", F_OK) == -1)
			createFile("judge-machine.notify", S_IRUSR);
	}
}

int main() {
	// Change directory to process executable directory
	string cwd;
	try {
		cwd = chdirToExecDir();
	} catch (const std::exception& e) {
		error_log("Failed to change working directory: ", e.what());
	}

	// Loggers
	try {
		stdlog.open("judge-machine.log");
	} catch (const std::exception& e) {
		error_log("Failed to open 'judge-machine.log': ", e.what());
	}

	try {
		error_log.open("judge-machine_error.log");
	} catch (const std::exception& e) {
		error_log("Failed to open 'judge-machine_error.log': ", e.what());
	}

	// Install signal handlers
	struct sigaction sa;
	memset (&sa, 0, sizeof(sa));
	sa.sa_handler = &exit;

	sigaction(SIGINT, &sa, nullptr);
	sigaction(SIGQUIT, &sa, nullptr);
	sigaction(SIGTERM, &sa, nullptr);

	try {
		// Connect to database
		db_conn = DB::createConnectionUsingPassFile(".db.config");
		// Create tmp_dir
		tmp_dir = TemporaryDirectory("/tmp/sim-judge-machine.XXXXXX");

	} catch (const std::exception& e) {
		error_log("Caugqht exception: ", __FILE__, ':', toString(__LINE__),
			" - ", e.what());
		return 1;
	}

	// Initialise inotify
	int inotify_fd, wd;
	while ((inotify_fd = inotify_init()) == -1) {
		error_log("Error: inotify_init()", error(errno));
		// Run tests
		processSubmissionQueue();
		usleep(OLD_WATCH_METHOD_SLEEP); // sleep
	}

	// If "judge-machine.notify" file does not exist create it
	if (access("judge-machine.notify", F_OK) == -1)
		createFile("judge-machine.notify", S_IRUSR);

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
			error_log("Error: read()", error(errno));
			continue;
		}

		// If notify file disappeared
		if (access("judge-machine.notify", F_OK) == -1) {
			createFile("judge-machine.notify", S_IRUSR);

			inotify_rm_watch(inotify_fd, wd);
			startWatching(inotify_fd, wd);
		}

		// Run tests
		processSubmissionQueue();
	}
	return 0;
}
