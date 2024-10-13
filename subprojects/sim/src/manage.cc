#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <simlib/am_i_root.hh>
#include <simlib/argv_parser.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/errmsg.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/logger.hh>
#include <simlib/path.hh>
#include <simlib/process.hh>
#include <simlib/string_view.hh>
#include <simlib/syscalls.hh>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

using std::string;

static void daemonize() {
    pid_t pid = fork();
    if (pid < 0) {
        errlog("fork()", errmsg());
        _exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        _exit(EXIT_SUCCESS);
    }

    // TODO: maybe log to files?

    if (setsid() == -1) {
        errlog("setsid()", errmsg());
        _exit(EXIT_FAILURE);
    }

    FileDescriptor fd{"/dev/null", O_RDWR};
    for (int fdx : {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO}) {
        if (fd != fdx and dup2(fd, fdx) == -1) {
            errlog("dup2()", errmsg());
            _exit(EXIT_FAILURE);
        }
    }
}

struct CmdOptions {
    bool make_background = false;
};

static CmdOptions parse_cmd_options(int& argc, char** argv) {
    CmdOptions cmd_options;
    int new_argc = 0;
    for (int i = 0; i < argc; ++i) {
        StringView arg = argv[i];
        if (arg == "-b") {
            cmd_options.make_background = true;
            continue;
        }
        argv[new_argc++] = argv[i];
    }
    argv[new_argc] = nullptr;
    argc = new_argc;
    return cmd_options;
}

namespace command {

static void help(const char* program_name) {
    if (not program_name) {
        program_name = "manage";
    }

    // clang-format off
    stdlog("Usage: ", program_name, " [options] <command> [<command args>]\n"
           "Manage is a tool for controlling the Sim instance.\n"
           "Commands:\n"
           "  help    Display this information\n"
           "  restart Kill current Sim instance, start a new one, and\n"
           "            indefinitely restart it if it dies\n"
           "  start   Same as restart\n"
           "  stop    Kill current Sim instance\n"
           "Options: \n"
           "  -b      Before monitoring Sim instance for restarts make it a\n"
           "            background process");
    // clang-format on
}

struct SimPaths {
    string manage = executable_path(getpid());
    string sim_server = concat_tostr(path_dirpath(manage), "bin/sim-server");
    string job_server = concat_tostr(path_dirpath(manage), "bin/job-server");
    string sim_upgrader = concat_tostr(path_dirpath(manage), "bin/sim-upgrader");
};

static void stop() {
    SimPaths paths;
    // First kill manage so that it won't restart servers
    kill_processes_by_exec({paths.manage}, std::chrono::seconds(1), true);
    // Kill servers
    kill_processes_by_exec({paths.sim_server, paths.job_server}, std::chrono::seconds(4), true);
}

static void restart(const CmdOptions& cmd_options) {
    // First, stop.
    stop();

    SimPaths paths;

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    if (pthread_sigmask(SIG_BLOCK, &mask, nullptr)) {
        errlog("pthread_sigmask()", errmsg());
        _exit(EXIT_FAILURE);
    }

    using std::chrono::system_clock;

    struct Server {
        pid_t pid = -1;
        // Respawn from file desciptor to prevent running a newer executable that may require e.g.
        // new database schema, but the database is not yet upgraded
        FileDescriptor executable_fd;
        system_clock::time_point last_spawn = system_clock::time_point::min();
    };

    auto spawn = [](Server& server) {
        while (true) {
            // Rate limiting
            auto now = system_clock::now();
            constexpr auto rate_limit = std::chrono::seconds(1);
            if (now < server.last_spawn + rate_limit) {
                std::this_thread::sleep_until(server.last_spawn + rate_limit);
            }

            pid_t pid = fork();
            if (pid < 0) {
                errlog("fork()", errmsg());
                server.last_spawn = system_clock::now();
                continue;
            }
            if (pid == 0) { // Child
                char* empty_arr[] = {nullptr};
                syscalls::execveat(server.executable_fd, "", empty_arr, environ, AT_EMPTY_PATH);
                _exit(EXIT_FAILURE); // execveat() failed
            }
            // Parent
            server.pid = pid;
            server.last_spawn = system_clock::now();
            break;
        }
    };
    std::array<Server, 2> servers = {{
        {-1, FileDescriptor{paths.sim_server, O_PATH | O_CLOEXEC}},
        {-1, FileDescriptor{paths.job_server, O_PATH | O_CLOEXEC}},
    }};

    // Run the sim upgrader.
    {
        auto pid = fork();
        if (pid < 0) {
            errlog("fork()", errmsg());
            _exit(EXIT_FAILURE);
        }
        if (pid == 0) {
            char* empty_arr[] = {nullptr};
            syscalls::execveat(AT_FDCWD, paths.sim_upgrader.c_str(), empty_arr, environ, 0);
            _exit(EXIT_FAILURE); // execveat() failed
        }
        siginfo_t si;
        if (syscalls::waitid(P_PID, pid, &si, WEXITED, nullptr)) {
            errlog("waitid()", errmsg());
            _exit(EXIT_FAILURE);
        }
        if (si.si_code != CLD_EXITED || si.si_status != 0) {
            errlog("Sim upgrade failed, aborting staring server.");
            _exit(EXIT_FAILURE);
        }
    }

    if (cmd_options.make_background) {
        daemonize();
    }

    for (auto& server : servers) {
        spawn(server);
    }

    for (;;) {
        siginfo_t si;
        if (syscalls::waitid(P_ALL, 0, &si, WEXITED, nullptr)) {
            errlog("waitid()", errmsg());
        }
        // Respawn server
        for (auto& server : servers) {
            if (server.pid == si.si_pid) {
                spawn(server);
                break;
            }
        }
    }
}

static void start(const CmdOptions& cmd_options) { restart(cmd_options); }

} // namespace command

static int run_command(const CmdOptions& cmd_options, int argc, char** argv) {
    ArgvParser args(argc - 1, argv + 1);
    auto command = args.extract_next();
    int res = 0;
    if (command == "help") {
        command::help(argv[0]);
    } else if (command == "restart") {
        command::restart(cmd_options);
    } else if (command == "start") {
        command::start(cmd_options);
    } else if (command == "stop") {
        command::stop();
    } else {
        errlog("Unknown command: ", command);
        res = 1;
    }
    return res;
}

int main(int argc, char** argv) {
    stdlog.use(stdout);
    stdlog.label(false);

    if (am_i_root() != AmIRoot::NO) {
        errlog("This program should not be run as root.");
        return 1;
    }

    auto cmd_options = parse_cmd_options(argc, argv);
    if (argc < 2) {
        command::help(argv[0]);
        return 1;
    }

    return run_command(cmd_options, argc, argv);
}
