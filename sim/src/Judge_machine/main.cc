#include "judge.h"
#include "main.h"

#include "../include/db.h"
#include "../include/debug.h"
#include "../include/string.h"

#include <cerrno>
#include <cppconn/prepared_statement.h>
#include <csignal>
#include <cstring>
#include <limits.h>
#include <sys/inotify.h>

using std::string;

static const int OLD_WATCH_METHOD_SLEEP = 1 * 1000000; // 1s
static DB::Connection *db_conn = NULL;
UniquePtr<TemporaryDirectory> tmp_dir;
unsigned VERBOSITY = 2; // 0 - quiet, 1 - normal, 2 or more - verbose

static inline DB::Connection& conn() { return *db_conn; }

static void processSubmissionQueue() {
	try {
		UniquePtr<sql::Statement> stmt(conn()->createStatement());

		// While submission queue is not empty
		for (;;) {
			UniquePtr<sql::ResultSet> res(stmt->executeQuery(
				"SELECT id, user_id, round_id, problem_id FROM submissions "
				"WHERE status='waiting' ORDER BY queued LIMIT 10"));
			if (res->rowsCount() == 0)
				return; // Queue is empty

			while (res->next()) {
				try {
					string submission_id = res->getString(1);
					string user_id = res->getString(2);
					string round_id = res->getString(3);
					string problem_id = res->getString(4);

					// Judge
					JudgeResult jres = judge(submission_id, problem_id);

					// Update submission
					putFileContents("submissions/" + submission_id,
						jres.content);

					// Update final
					UniquePtr<sql::PreparedStatement> pstmt;
					if (jres.status != JudgeResult::CERROR) {
						// Remove old final
						// From submissions_to_rounds
						pstmt.reset(conn()->prepareStatement("UPDATE submissions_to_rounds SET final=false WHERE submission_id=(SELECT id FROM submissions WHERE round_id=? AND user_id=? AND final=true LIMIT 1)"));
						pstmt->setString(1, round_id);
						pstmt->setString(2, user_id);
						pstmt->executeUpdate();

						// From submissions
						pstmt.reset(conn()->prepareStatement(
							"UPDATE submissions SET final=false "
							"WHERE round_id=? AND user_id=? AND final=true"));
						pstmt->setString(1, round_id);
						pstmt->setString(2, user_id);
						pstmt->executeUpdate();

						// Set new final in submissions_to_rounds
						stmt->executeUpdate("UPDATE submissions_to_rounds "
							"SET final=true WHERE submission_id=" +
								submission_id);
					}

					// Update submission
					pstmt.reset(conn()->prepareStatement("UPDATE submissions "
						"SET status=?, score=?, final=? WHERE id=?"));

					switch (jres.status) {
					case JudgeResult::OK:
						pstmt->setString(1, "ok");
						break;

					case JudgeResult::ERROR:
						pstmt->setString(1, "error");
						break;

					case JudgeResult::CERROR:
						pstmt->setString(1, "c_error");
						pstmt->setNull(2, 0);
						pstmt->setBoolean(3, false);
						break;

					case JudgeResult::JUDGE_ERROR:
						pstmt->setString(1, "judge_error");
						pstmt->setNull(2, 0);
						pstmt->setBoolean(3, false);

					}

					if (jres.status != JudgeResult::CERROR &&
							jres.status != JudgeResult::JUDGE_ERROR) {
						pstmt->setString(2, toString(jres.score));
						pstmt->setBoolean(3, true);
					}

					pstmt->setString(4, submission_id);
					pstmt->executeUpdate();

				} catch (const std::exception& e) {
					E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__,
						__LINE__, e.what());
				}
			}
		}

	} catch (const std::exception& e) {
		E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__, __LINE__,
			e.what());

	} catch (...) {
		E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
	}
}

void startWatching(int inotify_fd, int& wd) {
	while ((wd = inotify_add_watch(inotify_fd, "judge-machine.notify",
			IN_ATTRIB)) == -1) {
		eprintf("Error: inotify_add_watch() - %s\n", strerror(errno));
		// Run tests
		processSubmissionQueue();
		usleep(OLD_WATCH_METHOD_SLEEP); // sleep

		if (access("judge-machine.notify", F_OK) == -1)
			createFile("judge-machine.notify", S_IRUSR);
	}
}

int main() {
	// Install signal handlers
	struct sigaction sa;
	memset (&sa, 0, sizeof(sa));
	sa.sa_handler = &exit;

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	// Connect to database
	try {
		db_conn = DB::createConnectionUsingPassFile("db.config");

	} catch (const std::exception& e) {
		E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__, __LINE__,
			e.what());
		return 1;

	} catch (...) {
		E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
		return 1;
	}

	// Create tmp_dir
	tmp_dir.reset(new TemporaryDirectory("/tmp/sim-judge-machine.XXXXXX"));

	// Initialise inotify
	int inotify_fd, wd;
	while ((inotify_fd = inotify_init()) == -1) {
		eprintf("Error: inotify_init() - %s\n", strerror(errno));
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
			eprintf("Error: read() - %s\n", strerror(errno));
			continue;
		}

		// If notify file disappear
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
