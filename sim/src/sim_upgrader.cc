#include <sim/mysql.h>
#include <simlib/defer.h>
#include <simlib/process.h>
#include <simlib/spawner.h>

using std::vector;

namespace {

MySQL::Connection conn;
InplaceBuff<PATH_MAX> sim_build;

struct CmdOptions {
	bool reset_new_problems_time_limits = false;
};

} // namespace

static void print_help(const char* program_name) {
	if (not program_name)
		program_name = "sim-upgrader";

	errlog.label(false);
	errlog(
	   "Usage: ", program_name,
	   " [options] <sim_build>\n"
	   "  Where sim_build is a path to build directory of a Sim to upgrade\n"
	   "\n"
	   "Options:\n"
	   "  -h, --help            Display this information\n"
	   "  -q, --quiet           Quiet mode");
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

			} else { // Unknown
				eprintf("Unknown option: '%s'\n", argv[i]);
			}

		} else
			argv[new_argc++] = argv[i];
	}

	argc = new_argc;
	return cmd_options;
}

static int perform_upgrade() {
	STACK_UNWINDING_MARK;

	conn.update("RENAME TABLE files TO contest_files");

	stdlog("\033[1;32mSim upgrading is complete\033[m");
	return 0;
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
	(void)cmd_options;
	if (argc != 2) {
		print_help(argv[0]);
		return 1;
	}

	sim_build.append(argv[1]);
	if (not hasSuffix(sim_build, "/"))
		sim_build.append('/');

	try {
		// Get connection
		conn = MySQL::make_conn_with_credential_file(
		   concat(sim_build, ".db.config"));
	} catch (const std::exception& e) {
		errlog("\033[31mFailed to connect to database\033[m - ", e.what());
		return 1;
	}

	// Stop server and job server
	kill_processes_by_exec({concat_tostr(sim_build, "sim-server"),
	                        concat_tostr(sim_build, "job-server")});
	Defer servers_restorer([] {
		try {
			stdlog("Restoring servers");
		} catch (...) {
		}

		Spawner::run(
		   "sh",
		   {"sh", "-c", concat_tostr('"', sim_build, "sim-server\"& disown")});
		Spawner::run(
		   "sh",
		   {"sh", "-c", concat_tostr('"', sim_build, "job-server\"& disown")});
	});

	return perform_upgrade();
}

int main(int argc, char** argv) {
	try {
		return true_main(argc, argv);
	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		throw;
	}
}
