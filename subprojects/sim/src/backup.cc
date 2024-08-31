#include <chrono>
#include <sim/internal_files/old_internal_file.hh>
#include <sim/jobs/old_job.hh>
#include <sim/mysql/mysql.hh>
#include <sim/old_mysql/old_mysql.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/config_file.hh>
#include <simlib/file_info.hh>
#include <simlib/file_manip.hh>
#include <simlib/file_remover.hh>
#include <simlib/sim/problem_package.hh>
#include <simlib/spawner.hh>
#include <simlib/time.hh>
#include <simlib/working_directory.hh>

using std::string;
using std::vector;

/// @brief Displays help
static void help(const char* program_name) {
    if (program_name == nullptr) {
        program_name = "backup";
    }

    printf("Usage: %s [options]\n", program_name);
    puts("Make a backup of solutions and database contents");
}

int main2(int argc, char** argv) {
    if (argc != 1) {
        help(argc > 0 ? argv[0] : nullptr);
        return 1;
    }

    chdir_relative_to_executable_dirpath("..");

    auto run_command = [](vector<string> argv) {
        auto es = Spawner::run(argv[0], argv);
        if (es.si.code != CLD_EXITED || es.si.status != 0) {
            errlog(argv[0], " failed: ", es.message);
            _exit(1);
        }
    };

    // Remove temporary internal files that were not removed e.g. because of web/job server crash
    stdlog("Removing orphaned temporary internal files...");
    {
        using std::chrono::system_clock;
        using namespace std::chrono_literals;

        auto mysql = sim::mysql::Connection::from_credential_file(".db.config");
        auto transaction = mysql.start_repeatable_read_transaction();
        auto old_mysql = old_mysql::ConnectionView{mysql};

        // Remove internal files that do not have an entry in internal_files
        sim::PackageContents fc;
        fc.load_from_directory(sim::internal_files::dir);
        std::set<std::string, std::less<>> orphaned_files;
        fc.for_each_with_prefix("", [&](StringView file) {
            orphaned_files.emplace(file.to_string());
        });

        auto stmt = old_mysql.prepare("SELECT id FROM internal_files");
        stmt.bind_and_execute();
        InplaceBuff<32> file_id;
        stmt.res_bind_all(file_id);
        while (stmt.next()) {
            orphaned_files.erase(orphaned_files.find(file_id));
        }

        // Remove orphaned files that are older than 2h (not to delete files
        // that are just created but not committed)
        for (const std::string& file : orphaned_files) {
            struct stat64 st = {};
            auto file_path = concat(sim::internal_files::dir, file);
            if (stat64(file_path.to_cstr().data(), &st)) {
                if (errno == ENOENT) {
                    break;
                }

                THROW("stat64", errmsg());
            }

            if (system_clock::now() - get_modification_time(st) > 2h) {
                stdlog("Deleting: ", file);
                (void)unlink(file_path);
            }
        }

        transaction.commit();
    }
    stdlog("... done.");

    stdlog("Dumping the database...");
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

        run_command({
            "/usr/bin/mariadb-dump",
            concat_tostr("--user=", user.as_string()),
            concat_tostr("--password=", password.as_string()),
            concat_tostr("--host=", host.as_string()),
            "--single-transaction",
            "--result-file=dump.sql",
            db.as_string(),
        });
    }
    stdlog("... done.");

    if (chmod("dump.sql", S_0600)) {
        THROW("chmod()", errmsg());
    }

    run_command({"/usr/bin/git", "init", "--initial-branch", "main"});
    run_command({"/usr/bin/git", "config", "user.name", "bin/backup"});
    run_command({"/usr/bin/git", "config", "user.email", ""});
    // Optimize git for worktree with many files
    run_command({"/usr/bin/git", "config", "feature.manyFiles", "true"});
    // Tell git not to delta compress too big files
    run_command({"/usr/bin/git", "config", "core.bigFileThreshold", "16m"});
    // Tell git not to make internal files too large
    run_command({"/usr/bin/git", "config", "pack.packSizeLimit", "1g"});
    // Tell git not to repack big packs
    run_command({"/usr/bin/git", "config", "gc.bigPackThreshold", "500m"});
    // Tell git not to repack large number of big packs
    run_command({"/usr/bin/git", "config", "gc.autoPackLimit", "0"});
    // Makes fetching from this repository faster
    run_command({"/usr/bin/git", "config", "pack.window", "0"});
    // Prevent git from too excessive memory usage during git fetch (on remote) and git gc
    run_command({"/usr/bin/git", "config", "pack.deltaCacheSize", "128m"});
    run_command({"/usr/bin/git", "config", "pack.windowMemory", "128m"});
    run_command({"/usr/bin/git", "config", "pack.threads", "1"});
    // Disable automatic git gc
    run_command({"/usr/bin/git", "config", "gc.auto", "0"});

    run_command({"/usr/bin/git", "add", "--verbose", "dump.sql"});
    run_command({"/usr/bin/git", "add", "--verbose", "bin/", "lib/", "manage"});
    run_command({"/usr/bin/git", "add", "--verbose", "internal_files/"});
    run_command({"/usr/bin/git", "add", "--verbose", "logs/"});
    run_command({"/usr/bin/git", "add", "--verbose", "sim.conf", ".db.config"});
    run_command({"/usr/bin/git", "add", "--verbose", "static/"});
    run_command(
        {"/usr/bin/git",
         "commit",
         "-q",
         "-m",
         concat_tostr("Backup ", local_mysql_datetime(), " (", utc_mysql_datetime(), " UTC)")}
    );
    run_command({"/usr/bin/git", "--no-pager", "show", "--stat"});

    return 0;
}

int main(int argc, char** argv) {
    try {
        return main2(argc, argv);

    } catch (const std::exception& e) {
        ERRLOG_CATCH(e);
        return 1;
    }
}
