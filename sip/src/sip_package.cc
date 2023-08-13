#include "compilation_cache.hh"
#include "constants.hh"
#include "sip_error.hh"
#include "sip_package.hh"
#include "sipfile.hh"
#include "templates/templates.hh"
#include "utils.hh"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <limits>
#include <memory>
#include <poll.h>
#include <simlib/argv_parser.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/defer.hh>
#include <simlib/directory.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/file_info.hh>
#include <simlib/file_manip.hh>
#include <simlib/from_unsafe.hh>
#include <simlib/inotify.hh>
#include <simlib/libzip.hh>
#include <simlib/path.hh>
#include <simlib/process.hh>
#include <simlib/random.hh>
#include <simlib/ranges.hh>
#include <simlib/repeating.hh>
#include <simlib/sim/problem_package.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_view.hh>
#include <simlib/time.hh>
#include <simlib/unlinked_temporary_file.hh>
#include <simlib/utilities.hh>
#include <simlib/working_directory.hh>
#include <string>
#include <sys/inotify.h>
#include <unistd.h>

using std::set;
using std::string;
using std::vector;

namespace {
constexpr std::chrono::nanoseconds DEFAULT_TIME_LIMIT = std::chrono::seconds(5);
constexpr uint64_t DEFAULT_MEMORY_LIMIT = 512; // In MiB
} // namespace

std::chrono::nanoseconds SipPackage::get_default_time_limit() {
    STACK_UNWINDING_MARK;

    // Check Sipfile
    if (access("Sipfile", F_OK) == 0) {
        sipfile.load_default_time_limit();
        return sipfile.default_time_limit;
    }

    return DEFAULT_TIME_LIMIT;
}

void SipPackage::prepare_tests_files() {
    STACK_UNWINDING_MARK;

    if (tests_files.has_value()) {
        return;
    }

    tests_files = TestsFiles();
}

void SipPackage::prepare_judge_worker() {
    STACK_UNWINDING_MARK;

    if (jworker.has_value()) {
        return;
    }

    jworker = sim::JudgeWorker();
    jworker.value().checker_time_limit = CHECKER_TIME_LIMIT;
    jworker.value().checker_memory_limit = CHECKER_MEMORY_LIMIT;
    jworker.value().score_cut_lambda = SCORE_CUT_LAMBDA;

    jworker.value().load_package(".", full_simfile.dump());
}

static uint64_t test_name_hash(StringView test_name) {
    uint64_t hash = 0;
    for (unsigned char c : reverse_view(test_name)) {
        hash = (hash * 257 + c) % 71'777'214'294'589'669;
    }
    return hash;
}

void SipPackage::generate_test_input_file(const Sipfile::GenTest& test, CStringView in_file) const {
    STACK_UNWINDING_MARK;

    auto generator = [&] {
        if (has_prefix(test.generator, "sh:")) {
            return concat(substring(test.generator, 3));
        }

        if (sim::filename_to_lang(test.generator) == sim::SolutionLanguage::UNKNOWN) {
            log_warning(
                "generator source file `",
                test.generator,
                "` has an invalid extension. It will be treated as a shell "
                "command.\n"
                "  To remove this warning you have to choose one of the "
                "following options:\n"
                "    1. Change specified generator in Sipfile to a valid source "
                "file e.g. utils/gen.cpp\n"
                "    2. Rename the generator source file to have appropriate "
                "extension e.g. utils/gen.cpp\n"
                "    3. If it is a shell command or a binary file, prefix the "
                "generator with sh: in Sipfile - e.g. sh:echo"
            );

            return concat(test.generator);
        }

        return CompilationCache::compile(test.generator);
    }();

    stdlog("generating ", test.name, ".in...").flush_no_nl();
    // Prepare environment
    if (setenv("SIP_TEST_NAME", test.name.to_string().data(), true)) {
        throw SipError("setenv()", errmsg());
    }
    auto seed = test_name_hash(test.name) ^ sipfile.base_seed;
    if (setenv("SIP_TEST_SEED", to_string(seed).data(), true)) {
        throw SipError("setenv()", errmsg());
    }

    // Run generator
    FileDescriptor errfd = open_unlinked_tmp_file();
    auto print_errfd = [&] {
        auto offset = lseek64(errfd, 0, SEEK_CUR);
        if (offset == 0) {
            return;
        }
        // Check if the output ends with newline
        (void)lseek64(errfd, -1, SEEK_CUR);
        char c = 0;
        if (read(errfd, &c, 1) != 1) {
            throw SipError("read()", errmsg());
        }
        // Print errfd to stderr
        (void)lseek(errfd, 0, SEEK_SET);
        if (blast(errfd, STDERR_FILENO)) {
            throw SipError("blast()", errmsg());
        }
        if (c != '\n') {
            (void)fputc('\n', stderr);
        }
    };
    auto es = Spawner::run(
        "sh",
        {"sh", "-c", concat_tostr(generator, ' ', test.generator_args)},
        Spawner::Options(-1, FileDescriptor(in_file, O_WRONLY | O_CREAT | O_TRUNC), errfd)
    );

    if (es.si.code == CLD_EXITED and es.si.status == 0) { // OK
        stdlog(
            " \033[1;32mdone\033[m in ",
            to_string(floor_to_10ms(es.cpu_runtime), false),
            " [ RT: ",
            to_string(floor_to_10ms(es.runtime), false),
            " ]"
        );
        print_errfd();
    } else { // RTE
        stdlog(
            " \033[1;31mfailed\033[m in ",
            to_string(floor_to_10ms(es.cpu_runtime), false),
            " [ RT: ",
            to_string(floor_to_10ms(es.runtime), false),
            " ] (",
            es.message,
            ')'
        );
        print_errfd();
        throw SipError("failed to generate test: ", test.name, " seed: ", seed);
    }
}

sim::JudgeReport::Test
SipPackage::generate_test_output_file(const sim::Simfile::Test& test, SipJudgeLogger& logger) {
    STACK_UNWINDING_MARK;

    stdlog("generating ", test.name, ".out...").flush_no_nl();

    auto es =
        jworker.value().run_solution(test.in, test.out.value(), test.time_limit, test.memory_limit);

    sim::JudgeReport::Test res(
        test.name,
        sim::JudgeReport::Test::OK,
        es.cpu_runtime,
        test.time_limit,
        es.vm_peak,
        test.memory_limit,
        string{}
    );

    if (es.si.code == CLD_EXITED and es.si.status == 0 and res.runtime <= res.time_limit) {
        // OK
    } else if (res.runtime >= res.time_limit or es.runtime >= res.time_limit) {
        // TLE
        res.status = sim::JudgeReport::Test::TLE;
        res.comment = "Time limit exceeded";
    } else if (es.message == "Memory limit exceeded" or res.memory_consumed > res.memory_limit) {
        // MLE
        res.status = sim::JudgeReport::Test::MLE;
        res.comment = "Memory limit exceeded";
    } else {
        // RTE
        res.status = sim::JudgeReport::Test::RTE;
        res.comment = "Runtime error";
        if (!es.message.empty()) {
            back_insert(res.comment, " (", es.message, ')');
        }
    }

    // Move cursor back to the beginning of the line
    stdlog("\033[G").flush_no_nl();

    logger.test(test.name, res, es);
    return res;
}

void SipPackage::reload_simfile_from_str(string contents) {
    try {
        simfile = sim::Simfile(std::move(contents));
    } catch (const std::exception& e) {
        throw SipError("(Simfile) ", e.what());
    }
}

void SipPackage::reload_sipfile_from_str(string contents) {
    try {
        sipfile = Sipfile(std::move(contents));
    } catch (const std::exception& e) {
        throw SipError("(Sipfile) ", e.what());
    }
}

SipPackage::SipPackage() {
    STACK_UNWINDING_MARK;

    if (access("Simfile", F_OK) == 0) {
        simfile_contents = get_file_contents("Simfile");
        reload_simfile_from_str(simfile_contents);
    }
    if (access("Sipfile", F_OK) == 0) {
        sipfile_contents = get_file_contents("Sipfile");
        reload_sipfile_from_str(sipfile_contents);
    }
}

void SipPackage::warn_about_tests_not_specified_as_static_or_generated() {
    prepare_tests_files();
    for (const auto& [test_name, test] : tests_files->tests) {
        assert(test.in.has_value() or test.out.has_value());
        StringView test_file = test.in ? *test.in : test.out.value();
        if (sipfile.get_static_tests().find(test_name) == sipfile.get_static_tests().end() and
            sipfile.get_gen_tests().find(test_name) == sipfile.get_gen_tests().end())
        {
            log_warning(
                "test `",
                test_name,
                "` (file: ",
                test_file,
                ") is neither specified as static nor as generated in Sipfile"
            );
        }
    }
}

void SipPackage::generate_test_input_files() {
    STACK_UNWINDING_MARK;

    stdlog("\033[1;36mGenerating input files...\033[m");

    if (access("Sipfile", F_OK) != 0) {
        throw SipError("No Sipfile was found");
    }

    // Check for tests that are both static and generated
    for (StringView test : sipfile.get_static_tests()) {
        if (sipfile.get_gen_tests().find(test) != sipfile.get_gen_tests().end()) {
            log_warning(
                "test `",
                test,
                "` is specified as static and as generated - treating "
                "it as generated"
            );
        }
    }

    prepare_tests_files();

    // Ensure that static tests have their .in files
    for (StringView test : sipfile.get_static_tests()) {
        auto it = tests_files->tests.find(test);
        if (it == tests_files->tests.end() or not it->second.in.has_value()) {
            throw SipError("static test `", test, "` does not have a corresponding input file");
        }
    };

    CStringView in_dir;
    if (is_directory("tests/")) {
        in_dir = "tests/";
    } else if (mkdir(in_dir = "in/") == -1 and errno != EEXIST) {
        throw SipError("failed to create directory in/ (mkdir()", errmsg(), ')');
    }

    // Generate .in files
    if (not sipfile.get_gen_tests().empty()) {
        sipfile.load_base_seed();
    }
    for (const auto& test : sipfile.get_gen_tests()) {
        auto it = tests_files->tests.find(test.name);
        if (it == tests_files->tests.end() or not it->second.in.has_value()) {
            generate_test_input_file(
                test, CStringView{from_unsafe{concat_tostr(in_dir, test.name, ".in")}}
            );
        } else {
            generate_test_input_file(
                test, CStringView{from_unsafe{concat_tostr(it->second.in.value())}}
            );
        }
    }

    tests_files = std::nullopt; // Probably new .in files were just created
}

[[maybe_unused]] static auto test_output_file(StringView test_input_file) {
    STACK_UNWINDING_MARK;

    if (has_prefix(test_input_file, "in/")) {
        return concat<32>("out", test_input_file.substring(2, test_input_file.size() - 2), "out");
    }

    return concat<32>(test_input_file.substring(0, test_input_file.size() - 2), "out");
}

void SipPackage::remove_tests_with_no_input_file_from_limits_in_simfile() {
    STACK_UNWINDING_MARK;

    if (access("Simfile", F_OK) != 0) {
        return; // Nothing to do (no Simfile)
    }

    prepare_tests_files();

    // Remove tests that have no corresponding input file
    const auto& limits = simfile.config_file().get_var("limits").as_array();
    vector<string> new_limits;
    for (const auto& entry : limits) {
        StringView test_name = StringView(entry).extract_leading(std::not_fn(is_space<char>));
        auto it = tests_files->tests.find(test_name);
        if (it == tests_files->tests.end() or not it->second.in.has_value()) {
            continue;
        }

        new_limits.emplace_back(entry);
    }

    replace_variable_in_simfile("limits", from_unsafe{simfile.dump_limits_value()}, false);
}

void SipPackage::generate_test_output_files() {
    STACK_UNWINDING_MARK;

    simfile.load_interactive();
    if (simfile.interactive) {
        throw SipError("Generating output files is not possible for an "
                       "interactive problem");
    }

    stdlog("\033[1;36mGenerating output files...\033[m");

    prepare_tests_files();
    // Create out/ dir if needed
    for (const auto& [_, test] : tests_files->tests) {
        if (test.in.has_value() and has_prefix(*test.in, "in/")) {
            if (mkdir("out") == -1 and errno != EEXIST) {
                throw SipError("failed to create directory out/ (mkdir()", errmsg(), ')');
            }
            break;
        }
    }

    // We need to remove invalid entries from limits as Conver will give
    // warnings about them
    remove_tests_with_no_input_file_from_limits_in_simfile();
    // Warn about orphaned .out files
    for (const auto& [test_name, test] : tests_files->tests) {
        if (not test.in.has_value()) {
            log_warning("orphaned test out file: ", test.out.value());
        }
    }

    std::set<StringView> tests_excluded_from_generating_output; // (test name)
    for (const auto& test_name : sipfile.get_static_tests()) {
        auto it = tests_files->tests.find(test_name);
        if (it == tests_files->tests.end()) {
            log_warning("static test ", test_name, " has no files");
            continue;
        }
        auto& test = it->second;
        if (test.in.has_value() and !test.out.has_value()) {
            continue; // In this case test's output needs to be generated
        }
        tests_excluded_from_generating_output.emplace(test_name);
    }

    // Create (or truncate) .out files to generate, as rebuild_full_simfile()
    // needs both .in and .out files to exist to register the test
    for (const auto& [test_name, test] : tests_files->tests) {
        if (not test.in.has_value() or tests_excluded_from_generating_output.count(test_name)) {
            continue;
        }

        if (test.out.has_value()) {
            put_file_contents(concat(test.out.value()), "");
        } else {
            put_file_contents(test_output_file(test.in.value()), "");
        }
    }
    tests_files = std::nullopt; // New .out files may have been created

    rebuild_full_simfile(true);
    if (full_simfile.solutions.empty()) {
        throw SipError("no main solution was found");
    }

    compile_solution(full_simfile.solutions.front());

    // Generate .out files and construct judge reports for adjusting time limits
    sim::JudgeReport jrep1;
    sim::JudgeReport jrep2;
    SipJudgeLogger logger{full_simfile};
    logger.begin(false);
    for (const auto& group : full_simfile.tgroups) {
        auto p = sim::Simfile::TestNameComparator::split(group.tests[0].name);
        if (p.gid != "0") {
            logger.begin(true); // Assumption: initial tests come as first
        }

        auto& judge_report = (p.gid != "0" ? jrep2 : jrep1);
        judge_report.groups.emplace_back();
        for (const auto& test : group.tests) {
            if (tests_excluded_from_generating_output.count(test.name)) {
                continue; // Also note that time limit for these tests will be
                          // default time limit
            }
            judge_report.groups.back().tests.emplace_back(generate_test_output_file(test, logger));
        }
    }
    logger.end();

    // Adjust time limits
    sim::Conver::reset_time_limits_using_jugde_reports(
        full_simfile, jrep1, jrep2, conver_options(false).rtl_opts
    );
    jworker = std::nullopt; // Time limits have changed
}

void SipPackage::judge_solution(StringView solution) {
    STACK_UNWINDING_MARK;

    compile_solution(solution);
    compile_checker();

    // For the main solution, default time limits are used
    if (solution == full_simfile.solutions.front()) {
        auto default_time_limit = get_default_time_limit();
        for (auto& tgroup : full_simfile.tgroups) {
            for (auto& test : tgroup.tests) {
                test.time_limit = default_time_limit;
            }
        }

        jworker = std::nullopt; // Time limits have changed
    }

    prepare_judge_worker();

    stdlog('{');
    CompilationCache::load_solution(jworker.value(), solution);
    CompilationCache::load_checker(jworker.value());
    SipJudgeLogger jlogger{full_simfile};
    auto jrep1 = jworker.value().judge(false, jlogger);
    auto jrep2 = jworker.value().judge(true, jlogger);

    // Adjust time limits according to the main solution judge times
    if (solution == full_simfile.solutions.front()) {
        sim::Conver::reset_time_limits_using_jugde_reports(
            full_simfile, jrep1, jrep2, conver_options(false).rtl_opts
        );
        jworker = std::nullopt; // Time limits have changed
    }

    stdlog('}');
}

void SipPackage::compile_solution(StringView solution) {
    STACK_UNWINDING_MARK;

    prepare_judge_worker();
    stdlog("\033[1;34m", solution, "\033[m:");
    if (CompilationCache::is_cached(solution)) {
        stdlog("Solution is already compiled.");
    }

    CompilationCache::load_solution(jworker.value(), solution);
}

void SipPackage::compile_checker() {
    STACK_UNWINDING_MARK;

    prepare_judge_worker();
    CompilationCache::load_checker(jworker.value());
}

static bool is_elf_file(FilePath path) noexcept {
    FileDescriptor fd(path, O_RDONLY);
    if (not fd.is_open()) {
        return false;
    }

    constexpr StringView ELF_MAGIC = "\x7F"
                                     "ELF";
    InplaceBuff<ELF_MAGIC.size()> buff;
    buff.size = read_all(fd, buff.data(), ELF_MAGIC.size());
    return (buff == ELF_MAGIC);
}

static void remove_elf_files_and_empty_dirs() {
    InplaceBuff<PATH_MAX> path("./");
    // Returns whether there are remaining dir entries left or not
    auto process_dir = [&path](auto& self) -> bool {
        bool is_curr_dir_empty = true;
        for_each_dir_component(path, [&](dirent* file) {
            Defer path_guard = [&path, len = path.size] { path.size = len; };
            path.append(file->d_name);
            bool is_dir =
                (file->d_type == DT_UNKNOWN ? is_directory(path) : file->d_type == DT_DIR);
            if (is_dir) {
                path.append('/');
                bool empty = self(self);
                if (not empty != 0 or rmdir(FilePath(path))) {
                    is_curr_dir_empty = false;
                }
            } else if (not is_elf_file(path) or unlink(path)) {
                is_curr_dir_empty = false;
            }
        });

        return is_curr_dir_empty;
    };

    process_dir(process_dir);
}

void SipPackage::clean() {
    STACK_UNWINDING_MARK;

    stdlog("Cleaning...").flush_no_nl();

    CompilationCache::clear();
    if (remove_r("utils/latex/") and errno != ENOENT) {
        THROW("remove_r()", errmsg());
    }

    remove_elf_files_and_empty_dirs();
    stdlog(" done.");

    warn_about_tests_not_specified_as_static_or_generated();
}

void SipPackage::remove_generated_test_files() {
    STACK_UNWINDING_MARK;

    prepare_tests_files();
    stdlog("Removing generated test files...").flush_no_nl();
    // Remove generated .in and .out files
    for (const auto& test : sipfile.get_gen_tests()) {
        auto it = tests_files->tests.find(test.name);
        if (it != tests_files->tests.end()) {
            if (it->second.in.has_value()) {
                (void)remove(it->second.in.value().to_string());
            }
            if (it->second.out.has_value()) {
                (void)remove(it->second.out.value().to_string());
            }
        }
    }
    stdlog(" done.");
}

void SipPackage::remove_test_files_not_specified_in_sipfile() {
    STACK_UNWINDING_MARK;

    if (access("Sipfile", F_OK) != 0) {
        throw SipError("No Sipfile was found");
    }

    prepare_tests_files();
    stdlog("Removing test files that are not specified as generated or"
           " static...")
        .flush_no_nl();

    for (const auto& [test_name, test] : tests_files->tests) {
        const auto& static_tests = sipfile.get_static_tests();
        const auto& gen_tests = sipfile.get_gen_tests();
        bool is_static = static_tests.find(test_name) != static_tests.end();
        bool is_generated = gen_tests.find(test_name) != gen_tests.end();
        if (is_static or is_generated) {
            continue;
        }

        if (test.in.has_value()) {
            (void)remove(concat(test.in.value()));
        }
        if (test.out.has_value()) {
            (void)remove(concat(test.out.value()));
        }
    }

    tests_files = std::nullopt; // Probably some test files were just removed
    stdlog(" done.");
}

sim::Conver::Options SipPackage::conver_options(bool set_default_time_limits) {
    STACK_UNWINDING_MARK;

    sim::Conver::Options copts;
    copts.reset_time_limits_using_main_solution = set_default_time_limits;
    copts.ignore_simfile = false;
    copts.seek_for_new_tests = true;
    copts.reset_scoring = false;
    copts.require_statement = false;
    copts.rtl_opts.min_time_limit = MIN_TIME_LIMIT;
    copts.rtl_opts.solution_runtime_coefficient = SOLUTION_RUNTIME_COEFFICIENT;

    // Check Simfile
    if (access("Simfile", F_OK) == 0) {
        try {
            simfile.load_name();
        } catch (...) {
            log_warning("no problem name was specified in Simfile");
            copts.name = "";
        }
    } else {
        copts.memory_limit = DEFAULT_MEMORY_LIMIT;
        copts.name = "";
    }

    using std::chrono_literals::operator""ns;

    copts.max_time_limit = std::chrono::duration_cast<decltype(copts.max_time_limit)>(
        sim::Conver::solution_runtime_to_time_limit(
            get_default_time_limit(), SOLUTION_RUNTIME_COEFFICIENT, MIN_TIME_LIMIT
        ) +
        0.5ns
    );

    return copts;
}

void SipPackage::rebuild_full_simfile(bool set_default_time_limits) {
    STACK_UNWINDING_MARK;

    sim::Conver conver;
    conver.package_path(".");
    sim::Conver::ConstructionResult cr =
        conver.construct_simfile(conver_options(set_default_time_limits));

    // Filter out every line except warnings
    StringView report = conver.report();
    size_t line_beg = 0;
    size_t line_end = report.find('\n');
    size_t next_warning = report.find("warning");
    while (next_warning != StringView::npos) {
        if (next_warning < line_end) {
            stdlog(report.substring(line_beg, line_end));
        }

        if (line_end == StringView::npos or line_end + 1 == report.size()) {
            break;
        }

        line_beg = line_end + 1;
        line_end = report.find('\n', line_beg);
        if (next_warning < line_beg) {
            next_warning = report.find(line_beg, "warning");
        }
    }

    // Ignore the need for the main solution to set the time limits - the
    // time limits will be set to max_time_limit
    // throw_assert(cr.status == sim::Conver::Status::COMPLETE);

    full_simfile = std::move(cr.simfile);
    jworker = std::nullopt; // jworker has become outdated
}

void SipPackage::save_scoring() {
    STACK_UNWINDING_MARK;
    stdlog("Saving scoring...").flush_no_nl();

    replace_variable_in_simfile("scoring", from_unsafe{full_simfile.dump_scoring_value()}, false);

    stdlog(" done.");
}

void SipPackage::save_limits() {
    STACK_UNWINDING_MARK;
    stdlog("Saving limits...").flush_no_nl();

    replace_variable_in_simfile("limits", from_unsafe{full_simfile.dump_limits_value()}, false);

    stdlog(" done.");
}

void SipPackage::save_template(StringView template_name) {
    STACK_UNWINDING_MARK;

    auto save_from_template = [&](CStringView path, StringView description, const string& content) {
        if (path_exists(path)) {
            assert(!description.empty());
            throw SipError(
                "File ",
                path,
                " already exists. To replace it with the default ",
                description,
                " template, remove the file and try again."
            );
        }
        put_file_contents(path, content);
    };

    if (template_name == "statement") {
        simfile.load_name();
        simfile.load_global_memory_limit_only();
        (void)mkdir("doc");
        save_from_template(
            "doc/sim-statement.cls", "sim statement class", templates::sim_statement_cls()
        );
        save_from_template(
            "doc/statement.tex",
            "statement",
            templates::statement_tex(simfile.name.value(), simfile.global_mem_limit)
        );
    } else if (template_name == "checker") {
        simfile.load_interactive();
        (void)mkdir("check");
        save_from_template(
            "check/checker.cc",
            simfile.interactive ? "interactive checker" : "checker",
            simfile.interactive ? templates::interactive_checker_cc() : templates::checker_cc()
        );
    } else if (template_name == "gen") {
        (void)mkdir("utils");
        save_from_template("utils/gen.cc", "test generator", templates::gen_cc());
    } else {
        throw SipError("Unrecognized template name: ", template_name);
    }
}

static void compile_tex_file(StringView file) {
    STACK_UNWINDING_MARK;

    stdlog("\033[1mCompiling ", file, "\033[m");
    // Make pdflatex look for classes and other files in the file's directory
    setenv("TEXINPUTS", concat_tostr(".:", path_dirpath(file), ':').data(), true);
    // It is necessary (essential) to run latex two times
    for (int iter = 0; iter < 2; ++iter) {
        auto es = Spawner::run(
            "pdflatex",
            {"pdflatex", "-halt-on-error", "-output-dir=utils/latex", file.to_string()},
            {-1, STDOUT_FILENO, STDERR_FILENO}
        );

        if (es.si.code != CLD_EXITED or es.si.status != 0) {
            // During watching it is not intended to stop on compilation error
            errlog("\033[1m", file, ": \033[1;31mcompilation failed\033[m");
            return; // Second iteration makes no sense on compilation error
        }
    }

    auto src = concat("utils/latex/", path_filename(file).without_suffix(3), "pdf");
    auto dest = concat(file.without_suffix(3), "pdf");
    if (move(src, dest) == -1 and errno != ENOENT) {
        throw SipError("failed to rename file ", src, " into ", dest, " (move()", errmsg(), ')');
    }

    stdlog("\033[1m", file, ": \033[1;32mcompilation complete\033[m");
}

class SipWatchingLog : public WatchingLog {
public:
    void could_not_watch(const string& path, int errnum) override {
        log_warning("could not watch ", path, ": inotify_add_watch()", errmsg(errnum));
    }

    void read_failed(int errnum) override { log_warning("read()", errmsg(errnum)); }

    void started_watching(const string& file) override {
        stdlog("\033[1mStarted watching ", file, "\033[m");
    }
};

static void watch_tex_files(const set<string>& tex_files) {
    STACK_UNWINDING_MARK;
    using namespace std::chrono_literals;

    FileModificationMonitor monitor;
    for (auto& path : tex_files) {
        monitor.add_path(path, 10ms);
    }

    monitor.set_add_missing_files_retry_period(50ms);
    monitor.set_watching_log(std::make_unique<SipWatchingLog>());
    monitor.set_event_handler(compile_tex_file);
    monitor.watch();
}

void SipPackage::compile_tex_files(ArgvParser args, bool watch) {
    STACK_UNWINDING_MARK;

    const char* empty_str = "";
    set tex_files = files_matching_patterns(
        [](StringView str) { return has_suffix(str, ".tex"); },
        args.size() > 0 ? args : ArgvParser(1, &empty_str)
    );
    if (tex_files.empty()) {
        log_warning(
            "no .tex file ", args.size() > 0 ? "matching specified patterns " : "", "was found"
        );
        return;
    }

    if (mkdir_r("utils/latex") == -1) {
        throw SipError("failed to create directory utils/latex/ (mkdir_r()", errmsg(), ')');
    }

    for (const auto& file : tex_files) {
        compile_tex_file(file);
    }

    if (watch) {
        watch_tex_files(tex_files);
    }
}

void progress_callback(zip_t* /*unused*/, double p, void* /*unused*/) {
    using std::chrono::duration;
    stdlog(
        "\033[2K\033[GZipping... Progress: ",
        to_string(duration<int, std::deci>(int(p * 1e3)), false),
        '%'
    )
        .flush_no_nl();
}

void SipPackage::archive_into_zip(CStringView dest_file) {
    STACK_UNWINDING_MARK;

    stdlog("Zipping...").flush_no_nl();

    // Create zip
    {
        ZipFile zip(concat("../", dest_file, ".zip"), ZIP_CREATE | ZIP_TRUNCATE);
        zip_register_progress_callback_with_state(zip, 0.001, progress_callback, nullptr, nullptr);
        zip.dir_add(dest_file); // Add main directory

        sim::PackageContents pkg;
        pkg.load_from_directory(".");
        pkg.for_each_with_prefix("", [&](StringView path) {
            auto name = concat(dest_file, '/', path);
            if (name.back() == '/') {
                zip.dir_add(name);
            } else {
                zip.file_add(name, zip.source_file(concat(path)));
            }
        });

        zip.close();
    }

    stdlog("\033[2K\033[GZipping... done.");
}

void SipPackage::replace_variable_in_configfile(
    const ConfigFile& cf,
    FilePath configfile_path,
    StringView configfile_contents,
    StringView var_name,
    std::optional<StringView> replacement,
    bool escape_replacement
) {
    STACK_UNWINDING_MARK;

    std::ofstream out(configfile_path.data());
    const auto& var = cf.get_var(var_name);
    if (var.is_set()) {
        auto val_span = var.value_span();
        StringView before = configfile_contents.substring(0, val_span.beg);
        StringView after = configfile_contents.substring(val_span.end);
        // Delete variable
        if (not replacement) {
            // Remove remaining variable declaration
            before.remove_trailing([](char c) { return (c != '\n'); });
            // Remove line if empty
            if (has_prefix(after, "\n")) {
                after.extract_prefix(1);
            }

            out << before << after;
            return;
        }

        // Replace current variable value
        if (escape_replacement) {
            out << before << ConfigFile::escape_string(*replacement) << after;
        } else {
            out << before << *replacement << after;
        }

        return;
    }

    // New variable
    out << configfile_contents;
    if (not replacement) {
        return;
    }

    if (not has_suffix(configfile_contents, "\n")) {
        out << '\n';
    }

    out << var_name << ": ";
    if (escape_replacement) {
        out << ConfigFile::escape_string(*replacement);
    } else {
        out << *replacement;
    }
    out << '\n';
}

void SipPackage::replace_variable_in_configfile(
    const ConfigFile& cf,
    FilePath configfile_path,
    StringView configfile_contents,
    StringView var_name,
    const vector<string>& replacement
) {
    STACK_UNWINDING_MARK;

    std::ofstream out(configfile_path.data());
    auto print_replacement = [&] {
        out << "[";
        if (not replacement.empty()) {
            out << "\n\t" << ConfigFile::escape_string(replacement[0]);
            for (uint i = 1; i < replacement.size(); ++i) {
                out << ",\n\t" << ConfigFile::escape_string(replacement[i]);
            }
            out << '\n';
        }
        out << ']';
    };

    const auto& var = cf.get_var(var_name);
    if (var.is_set()) {
        auto val_span = var.value_span();
        StringView before = configfile_contents.substring(0, val_span.beg);
        StringView after = configfile_contents.substring(val_span.end);

        out << before;
        print_replacement();
        out << after;

    } else {
        out << configfile_contents;
        if (not has_suffix(configfile_contents, "\n")) {
            out << '\n';
        }

        out << var_name << ": ";
        print_replacement();
        out << '\n';
    }
}

void SipPackage::replace_variable_in_simfile(
    StringView var_name, std::optional<StringView> replacement, bool escape_replacement
) {
    STACK_UNWINDING_MARK;

    replace_variable_in_configfile(
        simfile.config_file(),
        "Simfile",
        simfile_contents,
        var_name,
        replacement,
        escape_replacement
    );
    simfile_contents = get_file_contents("Simfile");
    reload_simfile_from_str(simfile_contents);
}

void SipPackage::replace_variable_in_simfile(
    StringView var_name, const vector<string>& replacement
) {
    STACK_UNWINDING_MARK;

    replace_variable_in_configfile(
        simfile.config_file(), "Simfile", simfile_contents, var_name, replacement
    );
    simfile_contents = get_file_contents("Simfile");
    reload_simfile_from_str(simfile_contents);
}

void SipPackage::replace_variable_in_sipfile(
    StringView var_name, StringView replacement, bool escape_replacement
) {
    STACK_UNWINDING_MARK;

    replace_variable_in_configfile(
        sipfile.config_file(),
        "Sipfile",
        sipfile_contents,
        var_name,
        replacement,
        escape_replacement
    );
    sipfile_contents = get_file_contents("Sipfile");
    reload_sipfile_from_str(sipfile_contents);
}

void SipPackage::replace_variable_in_sipfile(
    StringView var_name, const vector<string>& replacement
) {
    STACK_UNWINDING_MARK;

    replace_variable_in_configfile(
        sipfile.config_file(), "Sipfile", sipfile_contents, var_name, replacement
    );
    sipfile_contents = get_file_contents("Sipfile");
    reload_sipfile_from_str(sipfile_contents);
}

void SipPackage::create_default_directory_structure() {
    STACK_UNWINDING_MARK;

    stdlog("Creating directories...").flush_no_nl();

    if ((mkdir("check") == -1 and errno != EEXIST) or (mkdir("doc") == -1 and errno != EEXIST) or
        (mkdir("in") == -1 and errno != EEXIST) or (mkdir("out") == -1 and errno != EEXIST) or
        (mkdir("prog") == -1 and errno != EEXIST) or (mkdir("utils") == -1 and errno != EEXIST))
    {
        THROW("mkdir()", errmsg());
    }

    stdlog(" done.");
}

void SipPackage::create_default_sipfile() {
    STACK_UNWINDING_MARK;

    if (access("Sipfile", F_OK) == 0) {
        stdlog("Sipfile already created");
        return;
    }

    using nl = std::numeric_limits<decltype(Sipfile::base_seed)>;
    auto base_seed = get_random(nl::min(), nl::max());

    stdlog("Creating Sipfile...").flush_no_nl();
    // clang-format off
	const auto default_sipfile_contents = concat_tostr(
	   "default_time_limit: ", to_string(DEFAULT_TIME_LIMIT), "\n"
	   "static: [\n"
	       "\t# Here provide tests that are \"hard-coded\"\n"
	       "\t# Syntax: <test-range>\n"
	   "]\n",
	   "base_seed: ", to_string(base_seed), "\n"
	   "gen: [\n"
	       "\t# Here provide rules to generate tests\n"
	       "\t# Syntax: <test-range> <generator> [generator arguments]\n"
	   "]\n");
    // clang-format on

    reload_sipfile_from_str(default_sipfile_contents);
    put_file_contents("Sipfile", default_sipfile_contents);

    stdlog(" done.");
}

void SipPackage::create_default_simfile(std::optional<CStringView> problem_name) {
    STACK_UNWINDING_MARK;

    stdlog("Creating Simfile...").flush_no_nl();

    auto package_dir_filename =
        path_filename(StringView{from_unsafe{get_cwd()}}.without_suffix(1)).to_string();
    if (access("Simfile", F_OK) == 0) {
        simfile_contents = get_file_contents("Simfile");
        reload_simfile_from_str(simfile_contents);

        // Problem name
        auto replace_name_in_simfile = [&](StringView replacement) {
            stdlog("Overwriting the problem name with: ", replacement);
            replace_variable_in_simfile("name", replacement);
        };

        if (problem_name.has_value()) {
            replace_name_in_simfile(problem_name.value());
        } else {
            try {
                simfile.load_name();
            } catch (const std::exception& e) {
                stdlog("\033[1;35mwarning\033[m: ignoring: ", e.what());

                if (package_dir_filename.empty()) {
                    throw SipError("problem name was neither found in Simfile, nor "
                                   "provided as a command-line argument nor deduced from "
                                   "the provided package path");
                }

                replace_name_in_simfile(package_dir_filename);
            }
        }

        // Memory limit
        constexpr char mem_limit_var_name[] = "memory_limit";
        if (not simfile.config_file().get_var(mem_limit_var_name).is_set()) {
            replace_variable_in_simfile(
                mem_limit_var_name, from_unsafe{to_string(DEFAULT_MEMORY_LIMIT)}
            );
        }

    } else if (problem_name.has_value()) {
        put_file_contents(
            "Simfile",
            from_unsafe{concat(
                "name: ",
                ConfigFile::escape_string(problem_name.value()),
                "\nmemory_limit: ",
                DEFAULT_MEMORY_LIMIT,
                '\n'
            )}
        );

    } else if (not package_dir_filename.empty()) {
        put_file_contents(
            "Simfile",
            from_unsafe{concat(
                "name: ",
                ConfigFile::escape_string(package_dir_filename),
                "\nmemory_limit: ",
                DEFAULT_MEMORY_LIMIT,
                '\n'
            )}
        );

    } else { // iff absolute path of dir is /
        throw SipError("problem name was neither provided as a command-line"
                       " argument nor deduced from the provided package path");
    }

    reload_simfile_from_str(get_file_contents("Simfile"));

    stdlog(" done.");

    simfile.load_name();
    simfile.load_global_memory_limit_only();
}
