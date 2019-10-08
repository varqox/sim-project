#include "contest_entry_tokens.hh"
#include "contest_files.hh"
#include "contest_problems.hh"
#include "contest_rounds.hh"
#include "contest_users.hh"
#include "contests.hh"
#include "internal_files.hh"
#include "jobs.hh"
#include "problem_tags.hh"
#include "problems.hh"
#include "sessions.hh"
#include "submissions.hh"
#include "users.hh"

#include <iostream>
#include <sim/mysql.h>
#include <simlib/config_file.h>
#include <simlib/defer.h>
#include <simlib/process.h>
#include <simlib/spawner.h>

using std::vector;

static void load_tables_from_other_sim_backup() {
	STACK_UNWINDING_MARK;
	stdlog("Loading dump.sql...");
	ConfigFile cf;
	cf.add_vars("user", "password", "db");
	cf.load_config_from_file(concat(main_sim_build, ".db.config"));

	FileDescriptor fd(concat(other_sim_build, "dump.sql"), O_RDONLY);
	if (not fd.opened())
		THROW("Failed to open ", other_sim_build, "dump.sql");

	Spawner::Options opts = {fd, STDOUT_FILENO, STDERR_FILENO};
	auto es = Spawner::run("mysql",
	                       {"mysql", "-u", cf["user"].as_string(),
	                        concat_tostr("-p", cf["password"].as_string()),
	                        cf["db"].as_string(), "-B"},
	                       opts);
	if (es.si.code != CLD_EXITED or es.si.status != 0)
		THROW("loading dump.sql failed: ", es.message);
}

namespace {

struct CmdOptions {
	bool reset_new_problems_time_limits = false;
};

} // namespace

static void print_help(const char* program_name) {
	if (not program_name)
		program_name = "sim-merger";

	errlog.label(false);
	errlog("Usage: ", program_name,
	       " [options] <sim_build> <other_sim_build_backup>\n"
	       "  Where sim_build is a path to build directory of a Sim to which "
	       "Sim with backup of build is placed in sim_other_backup_build path\n"
	       "\n"
	       "Options:\n"
	       "  -h, --help            Display this information\n"
	       "  -q, --quiet           Quiet mode\n"
	       "  -r, --reset-new-problems-time-limits\n"
	       "                        Adds jobs to reset added problems' time "
	       "limits");
}

static CmdOptions parse_cmd_options(int& argc, char** argv) {
	STACK_UNWINDING_MARK;

	int new_argc = 1;
	CmdOptions cmd_options;
	for (int i = 1; i < argc; ++i) {

		if (argv[i][0] == '-') {
			if (0 == strcmp(argv[i], "-h") or
			    0 == strcmp(argv[i], "--help")) { // Help
				print_help(argv[0]); // argv[0] is valid (argc > 1)
				exit(0);

			} else if (0 == strcmp(argv[i], "-q") or
			           0 == strcmp(argv[i], "--quiet")) { // Quiet mode
				stdlog.open("/dev/null");

			} else if (0 == strcmp(argv[i], "-r") or
			           0 ==
			              strcmp(argv[i], "--reset-new-problems-time-limits")) {
				cmd_options.reset_new_problems_time_limits = true;

			} else { // Unknown
				eprintf("Unknown option: '%s'\n", argv[i]);
			}

		} else
			argv[new_argc++] = argv[i];
	}

	argc = new_argc;
	return cmd_options;
}

static int true_main(int argc, char** argv) {
	STACK_UNWINDING_MARK;

	// Signal control
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;

	(void)sigaction(SIGINT, &sa, nullptr);
	(void)sigaction(SIGTERM, &sa, nullptr);
	(void)sigaction(SIGQUIT, &sa, nullptr);

	stdlog.use(stdout);
	errlog.use(stderr);

	CmdOptions cmd_options = parse_cmd_options(argc, argv);
	if (argc != 3) {
		print_help(argv[0]);
		return 1;
	}

	main_sim_build.append(argv[1]);
	if (not hasSuffix(main_sim_build, "/"))
		main_sim_build.append('/');

	other_sim_build.append(argv[2]);
	if (not hasSuffix(other_sim_build, "/"))
		other_sim_build.append('/');

	if (abspath(main_sim_build, getCWD().to_string()) ==
	    abspath(other_sim_build, getCWD().to_string())) {
		errlog.label(false);
		errlog("sim_build and other_sim_build_backup cannot refer to the same "
		       "directory");
		return 1;
	}

	try {
		// Get connection
		conn = MySQL::make_conn_with_credential_file(
		   concat(main_sim_build, ".db.config"));
	} catch (const std::exception& e) {
		errlog("\033[31mFailed to connect to database\033[m - ", e.what());
		return 1;
	}

	// Stop server and job server
	kill_processes_by_exec({concat_tostr(main_sim_build, "sim-server"),
	                        concat_tostr(main_sim_build, "job-server")});
	Defer servers_restorer([] {
		try {
			stdlog("Restoring servers");
		} catch (...) {
		}

		Spawner::run(
		   "sh", {"sh", "-c",
		          concat_tostr('"', main_sim_build, "sim-server\"& disown")});
		Spawner::run(
		   "sh", {"sh", "-c",
		          concat_tostr('"', main_sim_build, "job-server\"& disown")});
	});

	STACK_UNWINDING_MARK;
	// Ensure that other_sim_bulid/dump.sql exists
	if (access(concat(other_sim_build, "dump.sql"), F_OK) != 0) {
		stdlog("File ", other_sim_build,
		       "dump.sql does not exists -> asking git to restore it");
		auto es = Spawner::run("git", {"git", "-C", other_sim_build.to_string(),
		                               "checkout", "--", "dump.sql"});
		if (es.si.code != CLD_EXITED or es.si.status != 0) {
			errlog("Git failed: ", es.message);
			return 1;
		}
	}

	STACK_UNWINDING_MARK;
	// Rename all current tables
	vector<InplaceBuff<64>> renamed_tables;
	renamed_tables.reserve(tables.size());
	bool merge_successful = false;
	Defer table_rename_undoer([&] {
		STACK_UNWINDING_MARK;
		if (merge_successful)
			return;

		try {
			stdlog("Restoring tables");
			conn.update("SET FOREIGN_KEY_CHECKS=0");
		} catch (...) {
		}
		for (auto& table_name : renamed_tables) {
			try {
				conn.update("DROP TABLE IF EXISTS ", table_name);
			} catch (const std::exception& e) {
				ERRLOG_CATCH(e);
			}
			try {
				conn.update("RENAME TABLE ", main_sim_table_prefix, table_name,
				            " TO ", table_name);
			} catch (const std::exception& e) {
				ERRLOG_CATCH(e);
			}
		}

		try {
			conn.update("SET FOREIGN_KEY_CHECKS=1");
		} catch (...) {
		}
	});

	STACK_UNWINDING_MARK;
	stdlog("Renaming tables");
	conn.update("SET FOREIGN_KEY_CHECKS=0");
	for (auto table_name : tables) {
		conn.update("DROP TABLE IF EXISTS ", main_sim_table_prefix, table_name);
		conn.update("RENAME TABLE ", table_name, " TO ", main_sim_table_prefix,
		            table_name);
		renamed_tables.emplace_back(table_name);
	}
	conn.update("SET FOREIGN_KEY_CHECKS=1");

	load_tables_from_other_sim_backup();

	IdsFromMainAndOtherJobs ids_from_both_jobs;
	vector<MergerBase*> mergers;

	stdlog("\033[1;36mMerging internal_files\033[m...");
	InternalFilesMerger internal_files(ids_from_both_jobs);
	mergers.emplace_back(&internal_files);

	stdlog("\033[1;36mMerging users\033[m...");
	UsersMerger users(ids_from_both_jobs);
	mergers.emplace_back(&users);

	stdlog("\033[1;36mMerging sessions\033[m...");
	SessionsMerger sessions(ids_from_both_jobs, users);
	mergers.emplace_back(&sessions);

	stdlog("\033[1;36mMerging problems\033[m...");
	ProblemsMerger problems(ids_from_both_jobs, internal_files, users,
	                        cmd_options.reset_new_problems_time_limits);
	mergers.emplace_back(&problems);

	stdlog("\033[1;36mMerging problem tags\033[m...");
	ProblemTagsMerger problem_tags(ids_from_both_jobs, problems);
	mergers.emplace_back(&problem_tags);

	stdlog("\033[1;36mMerging contests\033[m...");
	ContestsMerger contests(ids_from_both_jobs);
	mergers.emplace_back(&contests);

	stdlog("\033[1;36mMerging contest rounds\033[m...");
	ContestRoundsMerger contest_rounds(ids_from_both_jobs, contests);
	mergers.emplace_back(&contest_rounds);

	stdlog("\033[1;36mMerging contest problems\033[m...");
	ContestProblemsMerger contest_problems(ids_from_both_jobs, contest_rounds,
	                                       contests, problems);
	mergers.emplace_back(&contest_problems);

	stdlog("\033[1;36mMerging contest users\033[m...");
	ContestUsersMerger contest_users(ids_from_both_jobs, users, contests);
	mergers.emplace_back(&contest_users);

	stdlog("\033[1;36mMerging contest files\033[m...");
	ContestFilesMerger contest_files(ids_from_both_jobs, internal_files,
	                                 contests, users);
	mergers.emplace_back(&contest_files);

	stdlog("\033[1;36mMerging contest entry tokens\033[m...");
	ContestEntryTokensMerger contest_entry_tokens(ids_from_both_jobs, contests);
	mergers.emplace_back(&contest_entry_tokens);

	stdlog("\033[1;36mMerging submissions\033[m...");
	SubmissionsMerger submissions(ids_from_both_jobs, internal_files, users,
	                              problems, contest_problems, contest_rounds,
	                              contests);
	mergers.emplace_back(&submissions);

	stdlog("\033[1;36mMerging jobs\033[m...");
	JobsMerger jobs(ids_from_both_jobs, internal_files, users, submissions,
	                problems, contests, contest_rounds, contest_problems);
	mergers.emplace_back(&jobs);

	stdlog("\033[1;32mEverything merged (unsaved so far)\033[m\nProceed with "
	       "saving merged data? [yes/no]");
	std::string answer;
	std::cin >> answer;
	if (answer != "yes") {
		stdlog("Answered no -> aborting");
		return 0;
	}

	std::vector<MergerBase*> saves_to_rollback;
	saves_to_rollback.reserve(mergers.size());
	Defer saves_rollbacker([&] {
		for (MergerBase* merger : saves_to_rollback)
			merger->rollback_saving_merged_outside_database();
	});

	stdlog("\033[1;36mSaving merged data:\033[m");
	conn.update("SET FOREIGN_KEY_CHECKS=0");
	for (MergerBase* merger : mergers) {
		stdlog("> \033[1;36m", merger->sql_table_name(), "\033[m...");
		merger->save_merged();
		saves_to_rollback.emplace_back(merger);
	}
	conn.update("SET FOREIGN_KEY_CHECKS=1");

	stdlog("\033[1;36mRunning after-saving hooks:\033[m");
	for (MergerBase* merger : mergers) {
		stdlog("> \033[1;36m", merger->sql_table_name(), "\033[m...");
		merger->run_after_saving_hooks();
	}

	stdlog("\033[1;32mSim merging is complete\033[m");
	saves_to_rollback.clear();
	merge_successful = true;

	stdlog("\033[1;33mRemoving old main tables\033[m");
	conn.update("SET FOREIGN_KEY_CHECKS=0");
	for (auto table_name : tables)
		conn.update("DROP TABLE ", main_sim_table_prefix, table_name);
	conn.update("SET FOREIGN_KEY_CHECKS=1");

	return 0;
}

// TODO: reset added problems time limits
int main(int argc, char** argv) {
	try {
		return true_main(argc, argv);
	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		throw;
	}
}
