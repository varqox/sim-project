#include <sim/constants.h>
#include <sim/db.h>
#include <simlib/process.h>
#include <simlib/sim/judge_worker.h>
#include <simlib/utilities.h>
#include <sys/inotify.h>

using sim::JudgeReport;
using sim::JudgeWorker;
using std::string;

static constexpr int OLD_WATCH_METHOD_SLEEP = 1e6; // 1 s
static DB::Connection db_conn;

static void processSubmissionQueue() {
	JudgeWorker jworker;
	jworker.setVerbosity(true);

	// While submission queue is not empty
	for (;;) try {
		// TODO: fix bug (rejudged submissions)
		DB::Result res = db_conn.executeQuery(
			"SELECT id, user_id, round_id, problem_id FROM submissions "
			"WHERE status=" SSTATUS_WAITING_STR " AND type IS NOT NULL "
			"ORDER BY queued LIMIT 1");
		if (!res.next())
			return; // Queue is empty

		string submission_id = res[1];
		string user_id = res[2];
		string round_id = res[3];
		string problem_id = res[4];

		stdlog("Judging submission ", submission_id, " (problem: ", problem_id,
			')');

		jworker.loadPackage("problems/" + problem_id,
			getFileContents(concat("problems/", problem_id, "/Simfile"))
		);

		// Variables
		SubmissionStatus status = SubmissionStatus::OK;
		string initial_report, final_report;
		int64_t total_score = 0;

		auto send_report = [&] {
			// Update submission
			DB::Statement stmt;
			// Special status
			if (isIn(status, {SubmissionStatus::COMPILATION_ERROR,
				              SubmissionStatus::CHECKER_COMPILATION_ERROR,
				              SubmissionStatus::JUDGE_ERROR}))
			{
				stmt = db_conn.prepare(
					// x.id - id of a submission which will have set
					//   type=FINAL (latest with non-fatal status)
					//   UNION with 0
					// y.id - id of a submissions which will have set
					//   type=NORMAL (dropped from type=FINAL)
					//   UNION with 0
					// z.id - submission_id
					//
					// UNION with 0 - because if x or y was empty then the
					//   whole query wouldn't be executed (and 0 is safe
					//   because no submission with id=0 exists)
					"UPDATE submissions s, "
						"((SELECT id FROM submissions WHERE user_id=? "
								"AND round_id=? AND type<=" STYPE_FINAL_STR " "
								"AND status<" SSTATUS_WAITING_STR " AND id!=? "
								"ORDER BY id DESC LIMIT 1) "
							"UNION (SELECT 0 AS id)) x, "
						"((SELECT id FROM submissions WHERE user_id=? "
								"AND round_id=? AND type=" STYPE_FINAL_STR ") "
							"UNION (SELECT 0 AS id)) y, "
						"(SELECT (SELECT ?) AS id) z "
					// Set type properly and other columns ONLY for just
					//   judged submission
					"SET s.type=IF(s.id=x.id," STYPE_FINAL_STR ","
						"IF(s.type<=" STYPE_FINAL_STR "," STYPE_NORMAL_STR
							",s.type)),"
						"s.status=IF(s.id=z.id,?,s.status),"
						"s.initial_report=IF(s.id=z.id,?,s.initial_report),"
						"s.final_report=IF(s.id=z.id,?,s.final_report),"
						"s.score=IF(s.id=z.id,NULL,s.score)"
					"WHERE s.id=x.id OR s.id=y.id OR s.id=z.id");

			// Normal status
			} else {
				stmt = db_conn.prepare(
					// x.id - id of a submission which will have set
					//   type=FINAL (latest with non-fatal status)
					//   UNION with 0
					// y.id - id of a submissions which will have set
					//   type=NORMAL (dropped from type=FINAL)
					//   UNION with 0
					// z.id - submission_id
					//
					// UNION with 0 - because if x or y was empty then the
					//   whole query wouldn't be executed (and 0 is safe
					//   because no submission with id=0 exists)
					"UPDATE submissions s, "
						"((SELECT id FROM submissions WHERE user_id=? "
								"AND round_id=? AND type<=" STYPE_FINAL_STR " "
								"AND (status<" SSTATUS_WAITING_STR " OR id=?) "
								"ORDER BY id DESC LIMIT 1) "
							"UNION (SELECT 0 AS id)) x, "
						"((SELECT id FROM submissions WHERE user_id=? AND "
								"round_id=? AND type=" STYPE_FINAL_STR ") "
							"UNION (SELECT 0 AS id)) y, "
						"(SELECT (SELECT ?) AS id) z "
					// Set type properly and other columns ONLY for just
					//   judged submission
					"SET s.type=IF(s.id=x.id," STYPE_FINAL_STR ","
						"IF(s.type<=" STYPE_FINAL_STR "," STYPE_NORMAL_STR
							",s.type)),"
						"s.status=IF(s.id=z.id,?,s.status),"
						"s.initial_report=IF(s.id=z.id,?,s.initial_report),"
						"s.final_report=IF(s.id=z.id,?,s.final_report),"
						"s.score=IF(s.id=z.id,?,s.score)"
					"WHERE s.id=x.id OR s.id=y.id OR s.id=z.id");

				stmt.setInt64(10, total_score);
			}

			stmt.setString(1, user_id);
			stmt.setString(2, round_id);
			stmt.setString(3, submission_id);
			stmt.setString(4, user_id);
			stmt.setString(5, round_id);
			stmt.setString(6, submission_id);
			stmt.setUInt(7, static_cast<uint>(status));
			stmt.setString(8, initial_report);
			stmt.setString(9, final_report);

			stmt.executeUpdate();

		};

		string compilation_errors;

		// Compile checker
		stdlog("Compiling checker...");
		if (jworker.compileChecker(CHECKER_COMPILATION_TIME_LIMIT,
			&compilation_errors, COMPILATION_ERRORS_MAX_LENGTH, PROOT_PATH))
		{
			stdlog("Checker compilation failed.");

			status = SubmissionStatus::CHECKER_COMPILATION_ERROR;
			initial_report = concat("<pre class=\"compilation-errors\">",
				htmlSpecialChars(compilation_errors), "</pre>");

			send_report();
			continue;
		}

		// Compile solution
		stdlog("Compiling solution...");
		if (jworker.compileSolution(concat("solutions/", submission_id, ".cpp"),
			SOLUTION_COMPILATION_TIME_LIMIT, &compilation_errors,
			COMPILATION_ERRORS_MAX_LENGTH, PROOT_PATH))
		{
			stdlog("Solution compilation failed.");

			status = SubmissionStatus::COMPILATION_ERROR;
			initial_report = concat("<pre class=\"compilation-errors\">",
				htmlSpecialChars(compilation_errors), "</pre>");

			send_report();
			continue;
		}

		// Creates xml report from JudgeReport
		auto construct_report = [](const JudgeReport& jr, bool final) -> string
		{
			if (jr.groups.empty())
				return {};

			string report = concat("<h2>", (final ? "Final" : "Initial"),
					" testing report</h2>"
				"<table class=\"table\">"
				"<thead>"
					"<tr>"
						"<th class=\"test\">Test</th>"
						"<th class=\"result\">Result</th>"
						"<th class=\"time\">Time [s]</th>"
						"<th class=\"memory\">Memory [KB]</th>"
						"<th class=\"points\">Score</th>"
					"</tr>"
				"</thead>"
				"<tbody>"
			);

			auto append_normal_columns = [&](const JudgeReport::Test& test) {
				auto asTdString = [](JudgeReport::Test::Status s) {
					switch (s) {
					case JudgeReport::Test::OK:
						return "<td class=\"status green\">OK</td>";
					case JudgeReport::Test::WA:
						return "<td class=\"status red\">Wrong answer</td>";
					case JudgeReport::Test::TLE:
						return "<td class=\"status yellow\">"
							"Time limit exceeded</td>";
					case JudgeReport::Test::MLE:
						return "<td class=\"status yellow\">"
							"Memory limit exceeded</td>";
					case JudgeReport::Test::RTE:
						return "<td class=\"status intense-red\">"
							"Runtime error</td>";
					case JudgeReport::Test::CHECKER_ERROR:
						return "<td class=\"status blue\">Checker error</td>";
					}

					throw_assert(false); // We shouldn't get here
				};
				back_insert(report,
					"<td>", htmlSpecialChars(test.name), "</td>",
					asTdString(test.status),
					"<td>", usecToSecStr(test.runtime, 2, false), " / ",
						usecToSecStr(test.time_limit, 2, false), "</td>"
					"<td>", toStr(test.memory_consumed >> 10), " / ",
						toStr(test.memory_limit >> 10), "</td>");
			};

			bool there_are_comments = false;
			for (auto&& group : jr.groups) {
				throw_assert(group.tests.size() > 0);
				// First row
				report += "<tr>";
				append_normal_columns(group.tests[0]);
				back_insert(report, "<td class=\"groupscore\" rowspan=\"",
					toStr(group.tests.size()), "\">", toString(group.score),
					" / ", toStr(group.max_score), "</td></tr>");
				// Other rows
				std::for_each(group.tests.begin() + 1, group.tests.end(),
					[&](const JudgeReport::Test& test) {
						report += "<tr>";
						append_normal_columns(test);
						report += "</tr>";
					});

				for (auto&& test : group.tests)
					there_are_comments |= !test.comment.empty();
			}

			report += "</tbody></table>";

			// Tests comments
			if (there_are_comments) {
				report += "<ul class=\"tests-comments\">";
				for (auto&& group : jr.groups)
					for (auto&& test : group.tests)
						if (test.comment.size())
							back_insert(report, "<li>"
								"<span class=\"test-id\">",
									htmlSpecialChars(test.name), "</span>",
								htmlSpecialChars(test.comment), "</li>");

				report += "</ul>";
			}

			return report;
		};

		try {
			// Judge
			JudgeReport rep1 = jworker.judge(false);
			JudgeReport rep2 = jworker.judge(true);
			// Make reports
			initial_report = construct_report(rep1, false);
			final_report = construct_report(rep2, true);
			// Count score
			for (auto&& rep : {rep1, rep2})
				for (auto&& group : rep.groups)
					total_score += group.score;

			/* Determine the submission status */

			#if __cplusplus > 201103L
			# warning "Use variadic generic lambda instead"
			#endif
			// Sets status to OK or first encountered error and modifies it
			// with func
			auto set_status = [&status] (const JudgeReport& jr,
				void(*func)(SubmissionStatus&))
			{
				for (auto&& group : jr.groups)
					for (auto&& test : group.tests)
						if (test.status != JudgeReport::Test::OK) {
							switch (test.status) {
							case JudgeReport::Test::WA:
								status = SubmissionStatus::WA; break;
							case JudgeReport::Test::TLE:
								status = SubmissionStatus::TLE; break;
							case JudgeReport::Test::MLE:
								status = SubmissionStatus::MLE; break;
							case JudgeReport::Test::RTE:
								status = SubmissionStatus::RTE; break;
							default:
								// We should not get here
								throw_assert(false);
							}

							func(status);
							return;
						}

				status = SubmissionStatus::OK;
				func(status);
			};

			// Search for CHECKER_ERROR
			for (auto&& rep : {rep1, rep2})
				for (auto&& group : rep.groups)
					for (auto&& test : group.tests)
						if (test.status == JudgeReport::Test::CHECKER_ERROR) {
							status = SubmissionStatus::JUDGE_ERROR;
							errlog("Checker error: submission ", submission_id,
								" (problem id: ", problem_id, ") test `",
								test.name, '`');

							send_report();
							continue;
						}

			static_assert((int)SubmissionStatus::OK < 8, "Needed below");
			static_assert((int)SubmissionStatus::WA < 8, "Needed below");
			static_assert((int)SubmissionStatus::TLE < 8, "Needed below");
			static_assert((int)SubmissionStatus::MLE < 8, "Needed below");
			static_assert((int)SubmissionStatus::RTE < 8, "Needed below");

			static_assert(((int)SubmissionStatus::OK << 3) ==
				(int)SubmissionStatus::INITIAL_OK, "Needed below");
			static_assert(((int)SubmissionStatus::WA << 3) ==
				(int)SubmissionStatus::INITIAL_WA, "Needed below");
			static_assert(((int)SubmissionStatus::TLE << 3) ==
				(int)SubmissionStatus::INITIAL_TLE, "Needed below");
			static_assert(((int)SubmissionStatus::MLE << 3) ==
				(int)SubmissionStatus::INITIAL_MLE, "Needed below");
			static_assert(((int)SubmissionStatus::RTE << 3) ==
				(int)SubmissionStatus::INITIAL_RTE, "Needed below");

			// Initial status
			set_status(rep1, [](SubmissionStatus& s) {
				// s has only final status, we want the same initial and
				// final status in s
				int x = static_cast<int>(s);
				s = static_cast<SubmissionStatus>((x << 3) | x);
			});
			// If initial tests haven't passed
			if (status != (SubmissionStatus::OK | SubmissionStatus::INITIAL_OK))
			{
				send_report();
				continue;
			}

			// Final status
			set_status(rep2, [](SubmissionStatus& s) {
				// Initial tests have passed, so add INITIAL_OK
				s = s | SubmissionStatus::INITIAL_OK;
			});

		} catch (const std::exception& e) {
			ERRLOG_CATCH(e);
			stdlog("Judge error.");

			status = SubmissionStatus::JUDGE_ERROR;
			initial_report = concat("<pre>", htmlSpecialChars(e.what()),
				"</pre>");
			final_report.clear();
		}

		send_report();

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
		errlog("Failed to open `", JUDGE_LOG, "`: ", e.what());
	}

	try {
		errlog.open(JUDGE_ERROR_LOG);
	} catch (const std::exception& e) {
		errlog("Failed to open `", JUDGE_ERROR_LOG, "`: ", e.what());
	}

	stdlog("Judge machine launch:\n"
		"PID: ", toStr(getpid()));

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

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return 1;
	}

	// Initialize inotify
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
