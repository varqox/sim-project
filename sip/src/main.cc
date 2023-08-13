#include "commands/commands.hh"
#include "sip_error.hh"
#include "sip_package.hh"

#include <simlib/directory.hh>
#include <simlib/proc_stat_file_contents.hh>
#include <simlib/signal_handling.hh>
#include <simlib/working_directory.hh>

bool sip_verbose = false; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

/**
 * Parses options passed to Sip via arguments
 * @param argc like in main (will be modified to hold the number of non-option
 * parameters)
 * @param argv like in main (holds arguments)
 */
static void parse_options(int& argc, char** argv) {
    STACK_UNWINDING_MARK;

    int new_argc = 1;

    for (int i = 1; i < argc; ++i) {

        if (argv[i][0] == '-') {
            if (0 == strcmp(argv[i], "-C") and i + 1 < argc) {
                // Working directory
                if (chdir(argv[++i]) == -1) {
                    (void)fprintf(stderr, "Error: chdir()%s\n", errmsg(errno).c_str());
                    _exit(1);
                }

            } else if (0 == strcmp(argv[i], "-h") or 0 == strcmp(argv[i], "--help")) {
                commands::help(argv[0]); // argv[0] is valid (argc > 1)
                _exit(0);

            } else if (0 == strcmp(argv[i], "-V") or 0 == strcmp(argv[i], "--version")) {
                commands::version();
                _exit(0);

            } else if (0 == strcmp(argv[i], "-v") or 0 == strcmp(argv[i], "--sip_verbose")) {
                sip_verbose = true;

            } else if (0 == strcmp(argv[i], "-q") or 0 == strcmp(argv[i], "--quiet")) {
                stdlog.open("/dev/null");

            } else { // Unknown
                (void)fprintf(stderr, "Unknown option: '%s'\n", argv[i]);
            }

        } else {
            argv[new_argc++] = argv[i];
        }
    }

    argc = new_argc;
    argv[argc] = nullptr;
}

static void run_command(int argc, char** argv) {
    STACK_UNWINDING_MARK;

    ArgvParser args(argc - 1, argv + 1);
    StringView command = args.extract_next();
    if (sip_verbose) {
        commands::version();
    }

    // Commands
    if (command == "checker") {
        return commands::checker(args);
    }
    if (command == "clean") {
        return commands::clean();
    }
    if (command == "devclean") {
        return commands::devclean();
    }
    if (command == "devzip") {
        return commands::devzip();
    }
    if (command == "doc") {
        return commands::doc(args);
    }
    if (command == "docwatch") {
        return commands::docwatch(args);
    }
    if (command == "gen") {
        return commands::gen();
    }
    if (command == "genin") {
        return commands::genin();
    }
    if (command == "genout") {
        return commands::genout();
    }
    if (command == "gentests") {
        return commands::gen();
    }
    if (command == "help") {
        return commands::help(argv[0]);
    }
    if (command == "init") {
        return commands::init(args);
    }
    if (command == "interactive") {
        return commands::interactive(args);
    }
    if (command == "label") {
        return commands::label(args);
    }
    if (command == "main-sol") {
        return commands::main_sol(args);
    }
    if (command == "mem") {
        return commands::mem(args);
    }
    if (command == "name") {
        return commands::name(args);
    }
    if (command == "prog") {
        return commands::prog(args);
    }
    if (command == "regen") {
        return commands::regen();
    }
    if (command == "regenin") {
        return commands::regenin();
    }
    if (command == "reseed") {
        return commands::reseed();
    }
    if (command == "save") {
        return commands::save(args);
    }
    if (command == "statement") {
        return commands::statement(args);
    }
    if (command == "templ") {
        return commands::template_command(args);
    }
    if (command == "template") {
        return commands::template_command(args);
    }
    if (command == "test") {
        return commands::test(args);
    }
    if (command == "unset") {
        return commands::unset(args);
    }
    if (command == "version") {
        return commands::version();
    }
    if (command == "zip") {
        return commands::zip();
    }

    throw SipError("unknown command: ", command);
}

int real_main(int argc, char** argv) {
    stdlog.use(stdout);
    stdlog.label(false);
    errlog.label(false);

    parse_options(argc, argv);

    if (argc < 2) {
        commands::help(argv[0]);
        return 1;
    }

    try {
        run_command(argc, argv);
    } catch (const SipError& e) {
        errlog("\033[1;31mError\033[m: ", e.what());
        return 1;
    } catch (const std::exception& e) {
        ERRLOG_CATCH(e);
        return 1;
    } catch (...) {
        ERRLOG_CATCH();
        return 1;
    }

    return 0;
}

void kill_every_child_process() {
    auto my_pid_str = to_string(getpid());
    for_each_dir_component("/proc/", [&](dirent* file) {
        auto pid_opt = str2num<pid_t>(file->d_name);
        if (not pid_opt or *pid_opt < 1) {
            return; // Not a process
        }
        pid_t pid = *pid_opt;

        auto proc_stat = ProcStatFileContents::get(pid);
        constexpr uint PPID_FID = 3;
        auto ppid_str = proc_stat.field(PPID_FID);
        if (ppid_str == my_pid_str) {
            stdlog("Killing child: ", pid);
            (void)kill(pid, SIGTERM);
        }
    });
}

void delete_zip_file_that_is_being_created() {
    auto zip_path_prefix = get_cwd();
    --zip_path_prefix.size; // Trim trailing '/'
    zip_path_prefix.append(".zip.");

    // Find and delete temporary zip file
    constexpr CStringView fd_dir = "/proc/self/fd/";
    for_each_dir_component(fd_dir, [&](dirent* file) {
        auto src = concat<64>(fd_dir, file->d_name);
        InplaceBuff<PATH_MAX + 1> dest; // +1 for trailing null byte
        auto rc = readlink(src.to_cstr().data(), dest.data(), decltype(dest)::max_static_size);
        if (rc == -1) {
            return; // Ignore errors, as nothing reasonable can be done
        }
        dest.size = rc;

        if (has_prefix(dest, zip_path_prefix)) {
            (void)unlink(dest.to_cstr());
        }
    });
}

void cleanup_before_getting_killed(int signum) {
    // print message: "Sip was just killed by signal %i - %s\n"
    try {
        errlog(
            "\nSip was just killed by signal ", sigabbrev_np(signum), " - ", sigdescr_np(signum)
        );
    } catch (...) {
    }

    // Prevent other errors from showing up in the console
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    try {
        kill_every_child_process();
    } catch (...) {
    }

    try {
        delete_zip_file_that_is_being_created();
    } catch (...) {
    }
}

int main(int argc, char** argv) {
    return handle_signals_while_running(
        [&] { return real_main(argc, argv); }, cleanup_before_getting_killed, SIGINT, SIGTERM
    );
}
