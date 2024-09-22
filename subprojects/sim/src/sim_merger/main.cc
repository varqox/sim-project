#include "merge.hh"
#include "sim_instance.hh"

#include <csignal>
#include <cstdio>
#include <exception>
#include <sim/db/schema.hh>
#include <sim/mysql/mysql.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/file_path.hh>
#include <simlib/from_unsafe.hh>
#include <simlib/logger.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/path.hh>
#include <simlib/process.hh>
#include <simlib/spawner.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_view.hh>
#include <string>
#include <unistd.h>
#include <vector>

static void help(const char* program_name) {
    if (!program_name) {
        program_name = "sim-merger";
    }

    printf("Usage: %s <path_to_other_sim_instance>\n", program_name);
    puts("Merge everything from path_to_other_sim_instance into this sim instance.");
}

int main(int argc, char** argv) {
    if (argc != 2) {
        help(argv[0]);
        return 1;
    }

    auto main_sim_path = concat_tostr(path_dirpath(from_unsafe{executable_path(getpid())}), "../");
    auto arg1 = StringView(argv[1]);
    auto other_sim_path = concat_tostr(arg1, &"/"[has_suffix(arg1, "/")]);

    auto run_command = [](const std::vector<std::string>& argv) {
        auto es = Spawner::run(argv[0], argv);
        return es.si.code == CLD_EXITED && es.si.status == 0;
    };

    stdlog("main sim path: ", main_sim_path);
    stdlog("other sim path: ", other_sim_path);

    stdlog("\033[1mStopping main sim\033[m");
    if (!run_command({concat_tostr(main_sim_path, "manage"), "stop"})) {
        errlog("\033[1;31mStopping main sim failed\033[m");
        return 1;
    }

    stdlog("\033[1mStopping other sim\033[m");
    if (!run_command({concat_tostr(other_sim_path, "manage"), "stop"})) {
        errlog("\033[1;31mStopping main sim failed\033[m");
        return 1;
    }

    bool merging_failed = false;
    try {
        stdlog("\033[1mConnecting to the main sim database\033[m");
        auto main_sim_mysql = sim::mysql::Connection::from_credential_file(
            concat_tostr(main_sim_path, ".db.config").c_str()
        );

        stdlog("\033[1mConnecting to the other sim database\033[m");
        auto other_sim_mysql = sim::mysql::Connection::from_credential_file(
            concat_tostr(other_sim_path, ".db.config").c_str()
        );

        stdlog("\033[1mChecking schemas\033[m");
        auto main_sim_normalized_schema = normalized(sim::db::get_db_schema(main_sim_mysql));
        auto other_sim_normalized_schema = normalized(sim::db::get_db_schema(other_sim_mysql));
        if (main_sim_normalized_schema != other_sim_normalized_schema) {
            errlog("\033[1;31mMaim sim's schema and other sim's DB schema do not match\033[m");
            return 1;
        }
        if (main_sim_normalized_schema != normalized(sim::db::get_schema())) {
            errlog("\033[1;31mSim merger is written for a different DB schema\033[m");
            return 1;
        }

        auto main_transaction = main_sim_mysql.start_repeatable_read_transaction();
        auto other_transaction = other_sim_mysql.start_repeatable_read_transaction();

        auto main_sim = SimInstance{
            .path = main_sim_path,
            .mysql = main_sim_mysql,
        };
        auto other_sim = SimInstance{
            .path = other_sim_path,
            .mysql = other_sim_mysql,
        };

        merge(main_sim, other_sim);
        main_transaction.commit();
    } catch (const std::exception& e) {
        ERRLOG_CATCH(e);
        merging_failed = true;
        stdlog(
            "\033[1;31mMerging failed, you need to restore main sim from backup with: ",
            main_sim_path,
            "bin/backup restore latest\033[m"
        );
    } catch (...) {
        ERRLOG_CATCH();
        merging_failed = true;
        stdlog(
            "\033[1;31mMerging failed, you need to restore main sim from backup with: ",
            main_sim_path,
            "bin/backup restore latest\033[m"
        );
    }

    if (!merging_failed) {
        stdlog(
            "\033[1;33mMain sim is not running, you can start it manually with: ",
            main_sim_path,
            "manage -b start\033[m"
        );
    }
    stdlog(
        "\033[1;33mOther sim is not running, you can start it manually with: ",
        other_sim_path,
        "manage -b start\033[m"
    );
}
