#include <csignal>
#include <cstdio>
#include <fcntl.h>
#include <optional>
#include <simlib/concat_tostr.hh>
#include <simlib/config_file.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/logger.hh>
#include <simlib/spawner.hh>
#include <simlib/string_view.hh>
#include <simlib/throw_assert.hh>
#include <simlib/working_directory.hh>
#include <string>
#include <unistd.h>
#include <vector>

static void help(const char* program_name) {
    if (!program_name) {
        program_name = "restore_backup";
    }

    printf("Usage: %s <REVISION>\n", program_name);
    puts("Restore sim from backup revision <REVISION>. To list available revisions use:");
    puts("git log --all");
    puts("E.g. if you want to restore the revision identified by line \"commit "
         "987f1889ed38a9ebc01f031ddb988f0f8d9a0c6a\", use the commit hash as a revision:");
    printf("  %s 987f1889ed38a9ebc01f031ddb988f0f8d9a0c6a\n", program_name);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        help(argc > 0 ? argv[0] : nullptr);
        return 1;
    }

    chdir_relative_to_executable_dirpath("..");

    auto revision = argv[1];

    auto run_command = [](std::vector<std::string> argv,
                          std::optional<int> stdin_fd = std::nullopt) {
        auto es = Spawner::run(
            argv[0],
            argv,
            Spawner::Options(stdin_fd.value_or(STDIN_FILENO), STDOUT_FILENO, STDERR_FILENO)
        );
        if (es.si.code != CLD_EXITED || es.si.status != 0) {
            errlog(argv[0], " failed: ", es.message);
            _exit(1);
        }
    };

    // Stops Sim so it doesn't run on the messed up (during restoration) database and files
    run_command({"./manage", "stop"});

    // Restore the database
    stdlog("Restoring database dump file...");
    run_command({"/usr/bin/git", "checkout", revision, "--", "dump.sql"});
    stdlog("... done.");
    {
        ConfigFile cf;
        cf.add_vars("user", "password", "db", "host");
        cf.load_config_from_file(".db.config");
        const auto& user = cf.get_var("user");
        throw_assert(user.is_set());
        const auto& password = cf.get_var("password");
        throw_assert(password.is_set());
        const auto& db = cf.get_var("db");
        throw_assert(db.is_set());
        const auto& host = cf.get_var("host");
        throw_assert(host.is_set());

        auto dump_sql_fd = FileDescriptor{"dump.sql", O_RDONLY};
        throw_assert(dump_sql_fd.is_open());

        stdlog("Restoring database...");
        run_command(
            {
                "/usr/bin/mariadb",
                concat_tostr("--user=", user.as_string()),
                concat_tostr("--password=", password.as_string()),
                concat_tostr("--database=", db.as_string()),
                concat_tostr("--host=", host.as_string()),
            },
            dump_sql_fd
        );
        stdlog("... done.");
    }

    // Restore all files except modifications to .db.config
    auto db_config = get_file_contents(".db.config");
    stdlog("Restoring files...");
    run_command({"/usr/bin/git", "checkout", revision, "--", "."});
    stdlog("... done.");
    put_file_contents(".db.config", db_config);
    // Remove extra files e.g. internal files (e.g. ones created after the backup was made)
    stdlog("Removing extra files files...");
    run_command({
        "/usr/bin/sh",
        "-c",
        "/usr/bin/git ls-files --others | /usr/bin/xargs rm -f",
    });
    stdlog("... done.");

    stdlog("\033[1;32mRestoring sim done.\033[m");
    stdlog(
        "\033[1;33mSim is not running. If needed, you have to start it manually with: ",
        StringView{argv[0]}.without_trailing([](char c) { return c != '/'; }
        ).without_suffix(strlen("bin/")),
        "manage start -b\033[m"
    );

    return 0;
}
