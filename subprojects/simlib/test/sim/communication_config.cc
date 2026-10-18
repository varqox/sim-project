#include <gtest/gtest.h>
#include <limits>
#include <simlib/file_contents.hh>
#include <simlib/file_manip.hh>
#include <simlib/libzip.hh>
#include <simlib/sim/communication_config.hh>
#include <simlib/sim/conver.hh>
#include <simlib/sim/judge_worker.hh>
#include <simlib/sim/simfile.hh>
#include <simlib/temporary_directory.hh>

static std::string settings(
    std::string manager = "manager/manager.cc",
    std::string instances = "2",
    std::string concurrent = "1"
) {
    return concat_tostr(
        "communication: true\nmanager: ",
        manager,
        "\ninstances: ",
        instances,
        "\nmax_concurrent: ",
        concurrent,
        '\n'
    );
}

static std::string complete_simfile(std::string communication = settings()) {
    return concat_tostr(
        "name: example\nlabel: ex\nstatement: doc/statement.txt\n",
        communication,
        "solutions: [prog/model.cc]\nmemory_limit: 64\nlimits: [ex1 1]\n",
        "scoring: [1 100]\ntests_files: [ex1 in/ex1.in]\n"
    );
}

// NOLINTNEXTLINE
TEST(CommunicationConfig, simfile_round_trip) {
    sim::Simfile sf(
        complete_simfile(settings("manager/manager.cc", "32", "8") + "min_instances: 2\n")
    );
    sf.load_all();
    ASSERT_TRUE(sf.communication);
    EXPECT_EQ(sf.communication->manager, "manager/manager.cc");
    EXPECT_EQ(sf.communication->instances, 32);
    EXPECT_EQ(sf.communication->min_instances, 2);
    EXPECT_EQ(sf.communication->max_concurrent, 8);
    EXPECT_TRUE(sf.interactive);
    EXPECT_FALSE(sf.checker);
    EXPECT_EQ(sf.dump().find("interactive:"), std::string::npos);
    EXPECT_NE(sf.dump().find("communication: true\n"), std::string::npos);
    EXPECT_EQ(sf.dump().find("protocol:"), std::string::npos);
    sim::Simfile restored(sf.dump());
    restored.load_all();
    EXPECT_EQ(restored.dump(), sf.dump());
    EXPECT_EQ(restored.communication->dump_settings(), sf.communication->dump_settings());
    ASSERT_EQ(restored.tgroups.size(), 1);
    ASSERT_EQ(restored.tgroups.front().tests.size(), 1);
    EXPECT_FALSE(restored.tgroups.front().tests.front().out);
}

// NOLINTNEXTLINE
TEST(CommunicationConfig, simfile_defaults_to_two_sequential_runs) {
    sim::Simfile sf(complete_simfile("communication: true\nmanager: manager/manager.cc\n"));
    sf.load_all();
    ASSERT_TRUE(sf.communication);
    EXPECT_EQ(sf.communication->instances, 2);
    EXPECT_FALSE(sf.communication->min_instances);
    EXPECT_EQ(sf.communication->max_concurrent, 1);
}

// NOLINTNEXTLINE
TEST(CommunicationConfig, simfile_flag_values) {
    for (auto flag : {"true", "TRUE", "on", "1"}) {
        SCOPED_TRACE(flag);
        sim::Simfile sf(complete_simfile(settings() + concat_tostr("communication: ", flag, '\n')));
        EXPECT_NO_THROW(sf.load_all());
        EXPECT_TRUE(sf.communication);
    }
    for (auto flag : {"false", "FALSE", "off", "0"}) {
        SCOPED_TRACE(flag);
        sim::Simfile sf(
            concat_tostr("communication: ", flag, "\nmanager: ../unused\ninstances: many\n")
        );
        EXPECT_NO_THROW(sf.load_communication());
        EXPECT_FALSE(sf.communication);
    }
    sim::Simfile absent("manager: ../unused\ninstances: many\n");
    EXPECT_NO_THROW(absent.load_communication());
    EXPECT_FALSE(absent.communication);
    for (auto flag : {"", "yes", "no", "2", "-1", "invalid", "[true]"}) {
        SCOPED_TRACE(flag);
        sim::Simfile sf(settings() + concat_tostr("communication: ", flag, '\n'));
        EXPECT_THROW(sf.load_communication(), std::exception);
    }
}

// NOLINTNEXTLINE
TEST(CommunicationConfig, simfile_rejects_invalid_settings) {
    for (auto invalid :
         {
             "manager: []\n",
             "manager: ''\n",
             "manager: ../manager.cc\n",
             "manager: /manager/main.cc\n",
             "manager: manager/\n",
             "manager: manager/../prog/main.cc\n",
             "manager: manager/./main.cc\n",
             "manager: manager//main.cc\n",
             "manager: manager/main.cc/\n",
             "instances: [2]\n",
             "instances: -1\n",
             "instances: 0\n",
             "instances: 33\n",
             "instances: 4294967296\n",
             "instances: two\n",
             "instances: ''\n",
             "min_instances: [1]\n",
             "min_instances: 0\n",
             "min_instances: 3\n",
             "min_instances: -1\n",
             "min_instances: 4294967296\n",
             "min_instances: few\n",
             "min_instances: ''\n",
             "max_concurrent: [1]\n",
             "max_concurrent: 0\n",
             "max_concurrent: -1\n",
             "max_concurrent: 3\n",
             "max_concurrent: 4294967296\n",
             "max_concurrent: many\n",
             "max_concurrent: ''\n",
             "checker: check/checker.cc\n",
             "checker: ''\n",
         })
    {
        SCOPED_TRACE(invalid);
        sim::Simfile sf(settings() + invalid);
        EXPECT_THROW(sf.load_communication(), std::exception);
    }
    sim::Simfile too_many_concurrent(settings("manager/manager.cc", "32", "9"));
    EXPECT_THROW(too_many_concurrent.load_communication(), std::exception);
    sim::Simfile long_manager(settings("manager/" + std::string(4096, 'x') + ".cc"));
    EXPECT_THROW(long_manager.load_communication(), std::exception);
    for (auto key : {"manager"}) {
        SCOPED_TRACE(key);
        auto contents = settings();
        auto begin = contents.find(concat_tostr(key, ':'));
        contents.erase(begin, contents.find('\n', begin) - begin + 1);
        sim::Simfile sf(contents);
        EXPECT_THROW(sf.load_communication(), std::exception);
    }
}

// NOLINTNEXTLINE
TEST(CommunicationConfig, simfile_uses_last_scalar_value) {
    sim::Simfile sf(
        settings() + "instances: 3\ninstances: 4\nmin_instances: 1\nmin_instances: 2\n"
    );
    sf.load_communication();
    ASSERT_TRUE(sf.communication);
    EXPECT_EQ(sf.communication->instances, 4);
    EXPECT_EQ(sf.communication->min_instances, 2);
}

// NOLINTNEXTLINE
TEST(CommunicationConfig, variable_instances_round_trip) {
    for (auto minimum : {1, 2, 32}) {
        SCOPED_TRACE(minimum);
        sim::Simfile sf(complete_simfile(
            settings("manager/manager.cc", "32") + concat_tostr("min_instances: ", minimum, '\n')
        ));
        sf.load_all();
        EXPECT_EQ(sf.communication->instances, 32);
        EXPECT_EQ(sf.communication->min_instances, minimum);
        sim::Simfile restored(sf.dump());
        restored.load_all();
        EXPECT_EQ(restored.communication->dump_settings(), sf.communication->dump_settings());
        EXPECT_EQ(restored.dump(), sf.dump());
    }
}

// NOLINTNEXTLINE
TEST(CommunicationConfig, memory_admission) {
    using Config = sim::CommunicationConfig;
    auto page_size = static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
    auto available = Config::default_session_memory_in_bytes - Config::relay_memory_in_bytes -
        Config::default_manager_memory_in_bytes - page_size;
    EXPECT_NO_THROW(Config::validate_memory_limits(8, available / 8 - page_size));
    EXPECT_THROW(Config::validate_memory_limits(8, available / 8 - page_size + 1), std::exception);
    EXPECT_THROW(Config::validate_memory_limits(0, 1), std::exception);
    EXPECT_THROW(Config::validate_memory_limits(1, 0), std::exception);
    EXPECT_THROW(Config::validate_memory_limits(1, 1, 0), std::exception);
    EXPECT_THROW(
        Config::validate_memory_limits(1, std::numeric_limits<uint64_t>::max()), std::exception
    );
    EXPECT_THROW(
        Config::validate_memory_limits(1, 1, std::numeric_limits<uint64_t>::max()), std::exception
    );
    EXPECT_THROW(Config::validate_memory_limits(1, 1, 1, 0), std::exception);
}

class CommunicationPackage : public testing::Test {
protected:
    TemporaryDirectory dir{"/tmp/communication-package.XXXXXX"};
    sim::Conver conver;
    sim::Conver::Options options;

    void write(StringView path, StringView contents) {
        put_file_contents(concat_tostr(dir.path(), path), contents);
    }

    std::string zip_package(StringView contents, bool include_manager = true) {
        auto archive = concat_tostr(dir.path(), "package.zip");
        ZipFile zip(archive, ZIP_CREATE | ZIP_TRUNCATE);
        zip.file_add("example/Simfile", zip.source_buffer(contents));
        for (auto path : {"doc/statement.txt", "prog/model.cc", "in/ex1.in", "out/ex1.out"}) {
            auto data = get_file_contents(concat_tostr(dir.path(), path));
            zip.file_add(concat_tostr("example/", path), zip.source_buffer(data));
        }
        if (include_manager) {
            auto data = get_file_contents(concat_tostr(dir.path(), "manager/manager.cc"));
            zip.file_add("example/manager/manager.cc", zip.source_buffer(data));
        }
        zip.close();
        return archive;
    }

    void SetUp() override {
        for (auto subdir : {"in", "out", "prog", "manager", "doc"}) {
            ASSERT_EQ(mkdir_r(concat_tostr(dir.path(), subdir)), 0);
        }
        write("Simfile", "name: example\nlabel: ex\nmemory_limit: 64\n");
        write("Simfile.communication", "unrelated metadata");
        write("in/ex1.in", "42\n");
        write("out/ex1.out", "42\n");
        write("prog/model.cc", "int main() {}\n");
        write("manager/manager.cc", "int main() {}\n");
        write("doc/statement.txt", "Example statement\n");
        conver.package_path(dir.path());
        options.require_statement = false;
        options.seek_for_new_tests = true;
        options.global_time_limit = std::chrono::seconds{1};
    }
};

// NOLINTNEXTLINE
TEST_F(CommunicationPackage, legacy_ignores_manifest) {
    auto original = conver.construct_simfile(options).simfile;
    write("Simfile.communication", "invalid [ manifest");
    auto invalid_manifest = conver.construct_simfile(options).simfile;
    EXPECT_EQ(original.dump(), invalid_manifest.dump());
    EXPECT_FALSE(original.interactive);
    EXPECT_EQ(original.solutions.size(), 2);
    EXPECT_TRUE(original.tgroups.front().tests.front().out.has_value());
}

// NOLINTNEXTLINE
TEST_F(CommunicationPackage, explicit_selection) {
    write("Simfile", from_unsafe{complete_simfile()});
    options.execution_format = sim::ExecutionFormat::Communication;
    auto sf = conver.construct_simfile(options).simfile;
    EXPECT_TRUE(sf.interactive);
    EXPECT_FALSE(sf.checker.has_value());
    ASSERT_EQ(sf.solutions.size(), 1);
    EXPECT_EQ(sf.solutions.front(), "prog/model.cc");
    EXPECT_FALSE(sf.tgroups.front().tests.front().out.has_value());
    sim::JudgeWorker worker;
    EXPECT_NO_THROW(
        worker.load_package(dir.path(), sf.dump(), sim::ExecutionFormat::Communication)
    );
    EXPECT_NO_THROW(worker.load_package(dir.path(), sf.dump()));
}

// NOLINTNEXTLINE
TEST_F(CommunicationPackage, ignore_malformed_legacy_simfile) {
    write("Simfile", "invalid [ simfile");
    options.ignore_simfile = true;
    options.name = "ignored";
    options.memory_limit = 64;
    EXPECT_NO_THROW(conver.construct_simfile(options));
    options.execution_format = sim::ExecutionFormat::Communication;
    EXPECT_THROW(conver.construct_simfile(options), std::exception);
}

// NOLINTNEXTLINE
TEST_F(CommunicationPackage, rejects_legacy_checker) {
    write("Simfile", from_unsafe{complete_simfile()});
    options.execution_format = sim::ExecutionFormat::Communication;
    ASSERT_EQ(mkdir_r(concat_tostr(dir.path(), "check")), 0);
    write("check/checker.cc", "int main() {}\n");
    EXPECT_THROW(conver.construct_simfile(options), std::exception);
}

// NOLINTNEXTLINE
TEST_F(CommunicationPackage, nested_zip_package) {
    write("Simfile", from_unsafe{complete_simfile()});
    options.execution_format = sim::ExecutionFormat::Communication;
    auto sf = conver.construct_simfile(options).simfile;
    auto archive = concat_tostr(dir.path(), "package.zip");
    ZipFile zip(archive, ZIP_CREATE | ZIP_TRUNCATE);
    zip.file_add("example/Simfile", zip.source_buffer(from_unsafe{sf.dump()}));
    for (auto path : {"Simfile.communication", "manager/manager.cc", "prog/model.cc", "in/ex1.in"})
    {
        auto contents = get_file_contents(concat_tostr(dir.path(), path));
        zip.file_add(concat_tostr("example/", path), zip.source_buffer(contents));
    }
    zip.close();
    conver.package_path(archive);
    auto result = conver.construct_simfile(options);
    EXPECT_EQ(result.pkg_main_dir, "example/");
    EXPECT_EQ(result.simfile.dump(), sf.dump());
    sim::JudgeWorker worker;
    EXPECT_NO_THROW(
        worker.load_package(archive, std::nullopt, sim::ExecutionFormat::Communication)
    );
    EXPECT_NO_THROW(worker.load_package(archive, sf.dump(), sim::ExecutionFormat::Communication));
    EXPECT_NO_THROW(worker.load_package(archive, std::nullopt));
}

// NOLINTNEXTLINE
TEST_F(CommunicationPackage, simfile_directory_infers_execution_format) {
    auto contents = complete_simfile();
    write("Simfile", from_unsafe{contents});
    write("Simfile.communication", "invalid ignored metadata");
    auto sf = conver.construct_simfile(options).simfile;
    ASSERT_TRUE(sf.communication);
    EXPECT_EQ(sf.communication->instances, 2);
    EXPECT_TRUE(sf.interactive);
    EXPECT_FALSE(sf.checker);
    ASSERT_EQ(sf.solutions.size(), 1);
    EXPECT_EQ(sf.solutions.front(), "prog/model.cc");
    EXPECT_FALSE(sf.tgroups.front().tests.front().out);
    sim::JudgeWorker worker;
    EXPECT_NO_THROW(worker.load_package(dir.path(), std::nullopt));
    EXPECT_NO_THROW(worker.load_package(dir.path(), sf.dump()));
}

// NOLINTNEXTLINE
TEST_F(CommunicationPackage, simfile_zip_infers_execution_format) {
    auto contents = complete_simfile(settings("manager/manager.cc", "32") + "min_instances: 2\n");
    auto archive = zip_package(from_unsafe{contents});
    conver.package_path(archive);
    auto result = conver.construct_simfile(options);
    EXPECT_EQ(result.pkg_main_dir, "example/");
    ASSERT_TRUE(result.simfile.communication);
    EXPECT_EQ(result.simfile.communication->instances, 32);
    EXPECT_EQ(result.simfile.communication->min_instances, 2);
    ASSERT_EQ(result.simfile.solutions.size(), 1);
    EXPECT_EQ(result.simfile.solutions.front(), "prog/model.cc");
    EXPECT_FALSE(result.simfile.tgroups.front().tests.front().out);
    sim::JudgeWorker worker;
    EXPECT_NO_THROW(worker.load_package(archive, std::nullopt));
    EXPECT_NO_THROW(worker.load_package(archive, result.simfile.dump()));
}

// NOLINTNEXTLINE
TEST_F(CommunicationPackage, simfile_false_remains_legacy) {
    write("Simfile", "name: example\nlabel: ex\nmemory_limit: 64\ncommunication: false\n");
    auto sf = conver.construct_simfile(options).simfile;
    EXPECT_FALSE(sf.communication);
    EXPECT_FALSE(sf.interactive);
    ASSERT_EQ(sf.solutions.size(), 2);
    EXPECT_TRUE(sf.tgroups.front().tests.front().out);
    sim::JudgeWorker worker;
    EXPECT_NO_THROW(worker.load_package(dir.path(), sf.dump()));
}

// NOLINTNEXTLINE
TEST_F(CommunicationPackage, simfile_false_rejects_communication_format) {
    auto contents = complete_simfile("communication: false\ninteractive: true\n");
    write("Simfile", from_unsafe{contents});
    options.execution_format = sim::ExecutionFormat::Communication;
    EXPECT_THROW(conver.construct_simfile(options), std::exception);
    sim::JudgeWorker worker;
    EXPECT_THROW(
        worker.load_package(dir.path(), contents, sim::ExecutionFormat::Communication),
        std::exception
    );
}

// NOLINTNEXTLINE
TEST_F(CommunicationPackage, simfile_rejects_missing_or_non_source_manager) {
    for (auto manager : {"manager/missing.cc", "manager/statement.txt"}) {
        SCOPED_TRACE(manager);
        write("manager/statement.txt", "not source\n");
        auto contents = complete_simfile(settings(manager));
        write("Simfile", from_unsafe{contents});
        conver.package_path(dir.path());
        EXPECT_THROW(conver.construct_simfile(options), std::exception);
        auto archive = zip_package(from_unsafe{contents}, false);
        conver.package_path(archive);
        EXPECT_THROW(conver.construct_simfile(options), std::exception);
    }
    auto contents = complete_simfile(settings("manager/statement.txt"));
    sim::JudgeWorker worker;
    EXPECT_THROW(worker.load_package(dir.path(), contents), std::exception);
}

// NOLINTNEXTLINE
TEST_F(CommunicationPackage, simfile_rejects_checker_conflict) {
    auto contents = complete_simfile(settings() + "checker: check/checker.cc\n");
    write("Simfile", from_unsafe{contents});
    EXPECT_THROW(conver.construct_simfile(options), std::exception);
    sim::JudgeWorker worker;
    EXPECT_THROW(worker.load_package(dir.path(), contents), std::exception);
    auto archive = zip_package(from_unsafe{contents});
    conver.package_path(archive);
    EXPECT_THROW(conver.construct_simfile(options), std::exception);
    EXPECT_THROW(worker.load_package(archive, std::nullopt), std::exception);
}

// NOLINTNEXTLINE
TEST_F(CommunicationPackage, simfile_cannot_be_ignored) {
    auto contents = complete_simfile();
    write("Simfile", from_unsafe{contents});
    options.ignore_simfile = true;
    options.name = "ignored";
    options.memory_limit = 64;
    EXPECT_THROW(conver.construct_simfile(options), std::exception);
    auto archive = zip_package(from_unsafe{contents});
    conver.package_path(archive);
    EXPECT_THROW(conver.construct_simfile(options), std::exception);
}

// NOLINTNEXTLINE
TEST_F(CommunicationPackage, communication_format_requires_simfile_flag) {
    options.execution_format = sim::ExecutionFormat::Communication;
    EXPECT_THROW(conver.construct_simfile(options), std::exception);
    auto contents = complete_simfile("interactive: true\nchecker: manager/manager.cc\n");
    write("Simfile", from_unsafe{contents});
    sim::JudgeWorker worker;
    EXPECT_THROW(
        worker.load_package(dir.path(), contents, sim::ExecutionFormat::Communication),
        std::exception
    );
    EXPECT_NO_THROW(worker.load_package(dir.path(), contents));
    auto archive = zip_package(from_unsafe{contents});
    EXPECT_THROW(
        worker.load_package(archive, std::nullopt, sim::ExecutionFormat::Communication),
        std::exception
    );
    EXPECT_NO_THROW(worker.load_package(archive, std::nullopt));
}

// NOLINTNEXTLINE
TEST_F(CommunicationPackage, rejects_excessive_memory_before_calibration) {
    options.execution_format = sim::ExecutionFormat::Communication;
    write("Simfile", from_unsafe{complete_simfile(settings("manager/manager.cc", "8", "8"))});
    options.memory_limit = 512;
    EXPECT_THROW(conver.construct_simfile(options), std::exception);
    options.execution_format = sim::ExecutionFormat::Legacy;
    write("Simfile", "name: example\nlabel: ex\nmemory_limit: 64\n");
    EXPECT_NO_THROW(conver.construct_simfile(options));
}

// NOLINTNEXTLINE
TEST_F(CommunicationPackage, worker_rechecks_manager_memory_override) {
    write("Simfile", from_unsafe{complete_simfile()});
    options.execution_format = sim::ExecutionFormat::Communication;
    options.memory_limit = 512;
    auto sf = conver.construct_simfile(options).simfile;
    sim::JudgeWorker worker({.checker_memory_limit_in_bytes = 2ULL << 30});
    EXPECT_THROW(
        worker.load_package(dir.path(), sf.dump(), sim::ExecutionFormat::Communication),
        std::exception
    );
}

// NOLINTNEXTLINE
TEST_F(CommunicationPackage, calibration_requires_complete_full_score) {
    write("Simfile", from_unsafe{complete_simfile()});
    options.execution_format = sim::ExecutionFormat::Communication;
    auto sf = conver.construct_simfile(options).simfile;
    sim::JudgeReport report;
    report.groups.emplace_back();
    report.groups.back().tests.emplace_back(
        "ex1",
        sim::JudgeReport::Test::OK,
        std::chrono::milliseconds{100},
        std::chrono::seconds{1},
        1024,
        64 << 20,
        ""
    );
    auto& test = report.groups.back().tests.back();
    auto reset = [&] {
        sim::Conver::reset_time_limits_using_jugde_reports(sf, {}, report, {});
    };
    EXPECT_THROW(reset(), std::exception);
    test.communication.emplace();
    test.communication->complete = true;
    test.score_ratio = 0.5;
    EXPECT_THROW(reset(), std::exception);
    test.score_ratio = 1;
    EXPECT_NO_THROW(reset());
    test.status = sim::JudgeReport::Test::WA;
    EXPECT_THROW(reset(), std::exception);
    auto legacy = sf;
    legacy.communication.reset();
    EXPECT_NO_THROW(sim::Conver::reset_time_limits_using_jugde_reports(legacy, {}, report, {}));
    report.groups.clear();
    EXPECT_THROW(reset(), std::exception);
}
