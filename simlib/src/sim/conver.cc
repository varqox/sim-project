#include <chrono>
#include <map>
#include <set>
#include <simlib/concat.hh>
#include <simlib/file_info.hh>
#include <simlib/libzip.hh>
#include <simlib/meta/max.hh>
#include <simlib/sim/conver.hh>
#include <simlib/sim/judge_worker.hh>
#include <simlib/sim/problem_package.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_view.hh>
#include <simlib/throw_assert.hh>
#include <simlib/utilities.hh>
#include <utility>

using std::pair;
using std::string;
using std::vector;

namespace sim {

Conver::ConstructionResult Conver::construct_simfile(const Options& opts, bool be_verbose) {
    STACK_UNWINDING_MARK;

    using std::chrono_literals::operator""ns;

    if (opts.memory_limit.has_value() and opts.memory_limit.value() <= 0) {
        THROW("If set, memory_limit has to be greater than 0");
    }
    if (opts.global_time_limit.has_value() and opts.global_time_limit.value() <= 0ns) {
        THROW("If set, global_time_limit has to be greater than 0");
    }
    if (opts.max_time_limit <= 0ns) {
        THROW("max_time_limit has to be greater than 0");
    }
    if (opts.rtl_opts.min_time_limit < 0ns) {
        THROW("min_time_limit has to be non-negative");
    }
    if (opts.rtl_opts.solution_runtime_coefficient < 0) {
        THROW("solution_runtime_coefficient has to be non-negative");
    }

    // Reset report_
    report_.str.clear();
    report_.log_to_stdlog_ = be_verbose;

    /* Load contents of the package */

    PackageContents pc;
    assert(path_exists(package_path_));
    if (is_directory(package_path_)) {
        throw_assert(!package_path_.empty());
        if (package_path_.back() != '/') {
            package_path_ += '/';
        }
        // From now on, package_path_ has trailing '/' iff it is a directory
        pc.load_from_directory(package_path_);

    } else {
        pc.load_from_zip(package_path_);
    }

    StringView main_dir = pc.main_dir(); // Find main directory if exists

    auto exists_in_pkg = [&](StringView file) {
        return (not file.empty() and pc.exists(from_unsafe{concat(main_dir, file)}));
    };

    // "utils/" directory is ignored by Conver
    pc.remove_with_prefix(from_unsafe{concat(main_dir, "utils/")});

    // Load the Simfile from the package
    Simfile sf;
    if (not opts.ignore_simfile and exists_in_pkg("Simfile")) {
        try {
            sf = Simfile([&] {
                if (package_path_.back() == '/') {
                    return get_file_contents(concat(package_path_, main_dir, "Simfile"));
                }

                ZipFile zip(package_path_, ZIP_RDONLY);
                return zip.extract_to_str(zip.get_index(concat(main_dir, "Simfile")));
            }());
        } catch (const ConfigFile::ParseError& e) {
            THROW(concat_tostr("(Simfile) ", e.what(), '\n', e.diagnostics()));
        }
    }

    // Name
    if (opts.name) {
        sf.name = opts.name.value();
        report_.append("Problem's name (specified in options): ", sf.name.value());
    } else {
        report_.append("Problem's name is not specified in options - loading it"
                       " from Simfile");
        sf.load_name();
        report_.append("  name loaded from Simfile: ", sf.name.value());
    }

    // Label
    if (opts.label) {
        sf.label = opts.label.value();
        report_.append("Problem's label (specified in options): ", sf.label.value());
    } else {
        report_.append("Problem's label is not specified in options - loading"
                       " it from Simfile");
        try {
            sf.load_label();
            report_.append("  label loaded from Simfile: ", sf.label.value());

        } catch (const std::exception& e) {
            report_.append(
                "  ",
                e.what(),
                " -> generating label from the"
                " problem's name"
            );

            sf.label = shorten_name(sf.name.value());
            report_.append("  label generated from name: ", sf.label.value());
        }
    }

    auto collect_files = [&pc](auto&& prefix, auto&& cond) {
        vector<StringView> res;
        pc.for_each_with_prefix(prefix, [&](StringView file) {
            if (cond(file)) {
                res.emplace_back(file);
            }
        });
        return res;
    };

    // Checker
    try {
        sf.load_checker();
        if (not sf.checker.has_value()) {
            report_.append("Missing checker in the package's Simfile -"
                           " searching for one");
        } else if (not exists_in_pkg(sf.checker.value())) {
            report_.append("Invalid checker specified in the package's Simfile"
                           " - searching for one");
            sf.checker = std::nullopt;
        }
    } catch (...) {
        report_.append("Invalid checker specified in the package's Simfile -"
                       " searching for one");
    }

    // Interactive
    if (opts.interactive.has_value()) {
        sf.interactive = opts.interactive.value();
    }

    if (not sf.checker.has_value()) {
        // Scan check/ and checker/ directory
        auto x = collect_files(concat(main_dir, "check/"), is_source);
        {
            auto y = collect_files(concat(main_dir, "checker/"), is_source);
            x.insert(x.end(), y.begin(), y.end());
        }
        if (!x.empty()) {
            // Choose the one with the shortest path to be the checker
            std::swap(x.front(), *std::min_element(x.begin(), x.end(), [](auto&& a, auto&& b) {
                          return a.size() < b.size();
                      }));

            sf.checker = x.front().substr(main_dir.size()).to_string();
            report_.append("Chosen checker: ", sf.checker.value());

        } else if (sf.interactive) {
            report_.append("No checker was found");
            THROW("No checker was found, but interactive problems require "
                  "checker");
        } else {
            report_.append("No checker was found - the default checker will be used");
        }
    }

    // Exclude check/ and checker/ directories from future searches
    pc.remove_with_prefix(from_unsafe{concat(main_dir, "check/")});
    pc.remove_with_prefix(from_unsafe{concat(main_dir, "checker/")});

    // Statement
    try {
        sf.load_statement();
    } catch (...) {
    }

    auto is_statement = [](StringView file) {
        return has_one_of_suffixes(file, ".pdf", ".md", ".txt");
    };

    if (sf.statement and exists_in_pkg(sf.statement.value()) and is_statement(sf.statement.value()))
    {
        // OK
    } else {
        report_.append(
            (sf.statement ? "Invalid" : "Missing"),
            " statement specified in the package's Simfile - searching for "
            "one"
        );

        // Scan doc/ directory
        auto x = collect_files(concat(main_dir, "doc/"), is_statement);
        if (x.empty()) {
            x = collect_files("", is_statement); // Scan whole directory tree
        }

        // If there is no statement
        if (x.empty()) {
            if (opts.require_statement) {
                THROW("No statement was found (recognized extensions: pdf, md, "
                      "txt)");
            }
            report_.append("\033[1;35mwarning\033[m: no statement was "
                           "found (recognized extensions: pdf, md, txt)");

        } else {
            // Prefer PDF to other formats, then the shorter paths
            // The ones with shorter paths are preferred
            std::swap(x.front(), *std::min_element(x.begin(), x.end(), [](auto&& a, auto&& b) {
                          return (
                              pair<bool, size_t>(not has_suffix(a, ".pdf"), a.size()) <
                              pair<bool, size_t>(not has_suffix(b, ".pdf"), b.size())
                          );
                      }));

            sf.statement = x.front().substr(main_dir.size()).to_string();
            report_.append("Chosen statement: ", sf.statement.value());
        }
    }

    // Solutions
    try {
        sf.load_solutions();
    } catch (...) {
    }
    {
        vector<std::string> solutions;
        std::set<StringView> solutions_set; // Used to detect and eliminate
                                            // solutions duplication
        for (auto&& sol : sf.solutions) {
            if (exists_in_pkg(sol) and solutions_set.find(sol) == solutions_set.end()) {
                solutions.emplace_back(sol);
                solutions_set.emplace(sol);
            } else {
                report_.append(
                    "\033[1;35mwarning\033[m: ignoring invalid"
                    " solution: `",
                    sol,
                    '`'
                );
            }
        }

        auto x = collect_files("", is_source);
        if (solutions.empty()) { // The main solution has to be set
            if (x.empty()) {
                THROW("No solution was found");
            }

            // Choose the one with the shortest path to be the main solution
            std::swap(x.front(), *std::min_element(x.begin(), x.end(), [](auto&& a, auto&& b) {
                          return a.size() < b.size();
                      }));
        }

        // Merge solutions
        for (StringView s : x) {
            s.remove_prefix(main_dir.size());
            auto [_, inserted] = solutions_set.emplace(s);
            if (inserted) {
                solutions.emplace_back(s.to_string());
            }
        }

        // Put the solutions into the Simfile
        sf.solutions = std::move(solutions);
    }

    struct TestProperties {
        std::optional<StringView> in;
        std::optional<StringView> out;
        std::optional<std::chrono::nanoseconds> time_limit;
        std::optional<uint64_t> memory_limit;
    };

    std::map<StringView, TestProperties> tests; // test name => test props

    // Load tests and limits form variable "limits"
    const auto& limits = sf.config["limits"];
    if (not opts.ignore_simfile and limits.is_set() and limits.is_array()) {
        for (const auto& str : limits.as_array()) {
            StringView test_name;
            TestProperties test;
            try {
                std::tie(test_name, test.time_limit, test.memory_limit) =
                    Simfile::parse_limits_item(str);
            } catch (const std::exception& e) {
                report_.append(
                    "\033[1;35mwarning\033[m: \"limits\": ignoring"
                    " unparsable item -> ",
                    e.what()
                );
                continue;
            }

            if (Simfile::TestNameComparator::split(test_name).gid.empty()) {
                report_.append(
                    "\033[1;35mwarning\033[m: \"limits\": ignoring"
                    " test `",
                    test_name,
                    "` because it has no group id in its name"
                );
                continue;
            }

            // Replace on redefinition
            tests.insert_or_assign(test_name, test);
        }
    }

    // Process test files found in the package
    pc.for_each_with_prefix("", [&](StringView path) {
        assert(has_prefix(path, main_dir));
        path.remove_prefix(main_dir.size());

        if (not(has_suffix(path, ".in") or (has_suffix(path, ".out") and not sf.interactive))) {
            return;
        }

        StringView test_name =
            path.substr(0, path.rfind('.')).extract_trailing([](char c) { return c != '/'; });

        auto it =
            opts.seek_for_new_tests ? tests.try_emplace(test_name).first : tests.find(test_name);
        if (it == tests.end()) {
            return; // There is no such test
        }
        auto& test = it->second;

        // Match test file with the test
        if (has_suffix(path, ".in")) { // Input file
            if (test.in.has_value()) {
                report_.append(
                    "\033[1;35mwarning\033[m: input file for test `",
                    test_name,
                    "` was found in more than one location: `",
                    test.in.value(),
                    "` and `",
                    path,
                    "`, choosing the latter"
                );
            }
            test.in = path;
        } else { // Output file
            assert(has_suffix(path, ".out"));
            assert(not sf.interactive);
            if (test.out.has_value()) {
                report_.append(
                    "\033[1;35mwarning\033[m: output file for test `",
                    test_name,
                    "` was found in more than one location: `",
                    test.out.value(),
                    "` and `",
                    path,
                    "`, choosing the latter"
                );
            }
            test.out = path;
        }
    });

    // Load test files (this one may overwrite the files form previous step)
    const auto& tests_files = sf.config["tests_files"];
    if (not opts.ignore_simfile and tests_files.is_set() and tests_files.is_array()) {
        for (const auto& str : tests_files.as_array()) {
            StringView test_name;
            std::optional<StringView> input;
            std::optional<StringView> output;
            try {
                std::tie(test_name, input, output) = Simfile::parse_test_files_item(str);
            } catch (const std::exception& e) {
                report_.append(
                    "\033[1;35mwarning\033[m: \"tests_files\":"
                    " ignoring unparsable item -> ",
                    e.what()
                );
                continue;
            }

            if (test_name.empty()) {
                continue; // Ignore empty entries
            }

            if (input.value().empty()) {
                report_.append(
                    "\033[1;35mwarning\033[m: \"tests_files\":"
                    " missing test input file for test `",
                    test_name,
                    "` - ignoring"
                );

                throw_assert(output.value().empty());
                continue; // Nothing more to do
            }

            auto path = concat(main_dir, input.value());
            if (not pc.exists(path)) {
                report_.append(
                    "\033[1;35mwarning\033[m: \"tests_files\": test"
                    " input file: `",
                    input.value(),
                    "` not found - ignoring file"
                );
                input = std::nullopt;
            }

            path = concat(main_dir, output.value());
            if (sf.interactive) {
                if (not output.value().empty()) {
                    report_.append(
                        "\033[1;35mwarning\033[m: \"tests_files\":"
                        " test output file: `",
                        input.value(),
                        "` specified,"
                        " but the problem is interactive - ignoring file"
                    );
                }
                output = std::nullopt;

            } else if (output.value().empty()) {
                report_.append(
                    "\033[1;35mwarning\033[m: \"tests_files\":"
                    " missing test output file for test `",
                    test_name,
                    "` - ignoring"
                );
                output = std::nullopt;

            } else if (not pc.exists(path)) {
                report_.append(
                    "\033[1;35mwarning\033[m: \"tests_files\": test"
                    " output file: `",
                    output.value(),
                    "` not found - ignoring file"
                );
                output = std::nullopt;
            }

            if (opts.seek_for_new_tests) {
                tests.try_emplace(test_name); // Add test if it does not exist
            }

            auto it = tests.find(test_name);
            if (it == tests.end()) {
                report_.append(
                    "\033[1;35mwarning\033[m: \"tests_files\": ignoring files "
                    "for test `",
                    test_name,
                    "` because no such test is specified in \"limits\""
                );
                continue;
            }

            TestProperties& test = it->second;
            if (input.has_value()) {
                test.in = input;
            }

            if (output.has_value()) {
                test.out = output;
            }
        }
    }

    // Remove tests that have at most one file set
    filter(tests, [&](const auto& p) {
        auto const& [test_name, test] = p;
        // Warn if the test was loaded from "limits"
        if (not test.in.has_value() and test.time_limit.has_value()) {
            report_.append(
                "\033[1;35mwarning\033[m: limits: ignoring test `",
                test_name,
                "` because it has no corresponding input file"
            );
        }
        // Warn if the test was loaded from "limits"
        if (not sf.interactive and not test.out.has_value() and test.time_limit.has_value()) {
            report_.append(
                "\033[1;35mwarning\033[m: limits: ignoring test `",
                test_name,
                "` because it has no corresponding output file"
            );
        }

        return test.in.has_value() and (sf.interactive or test.out.has_value());
    });

    if (not opts.memory_limit.has_value()) {
        if (opts.ignore_simfile) {
            THROW("Missing memory limit - global memory limit not specified in"
                  " options and simfile is ignored");
        }

        report_.append("Global memory limit not specified in options - loading"
                       " it from Simfile");

        sf.load_global_memory_limit_only();
        if (not sf.global_mem_limit.has_value()) {
            report_.append("Memory limit is not specified in the Simfile");
            report_.append("\033[1;35mwarning\033[m: no global memory limit is set");
        }
    }

    // Update the memory limits
    if (opts.memory_limit.has_value()) {
        // Convert from MiB to bytes
        sf.global_mem_limit = opts.memory_limit.value() << 20;
        for (auto& [_, test] : tests) {
            test.memory_limit = sf.global_mem_limit;
        }
    } else { // Give tests without specified memory limit the global memory
             // limit
        for (auto& [test_name, test] : tests) {
            if (not test.memory_limit.has_value()) {
                if (not sf.global_mem_limit.has_value()) {
                    THROW(
                        "Memory limit is not specified for test `",
                        test_name,
                        "` and no global memory limit is set"
                    );
                }
                test.memory_limit = sf.global_mem_limit;
            }
        }
    }

    struct TestsGroup {
        std::optional<int64_t> score;
        // test name => test props
        std::multimap<StringView, TestProperties, Simfile::TestNameComparator> tests;
    };

    std::map<StringView, TestsGroup, Simfile::TestNameComparator>
        tests_groups; // group name => test group
    // Fill tests_groups
    for (const auto& [test_name, test] : tests) {
        auto sr = Simfile::TestNameComparator::split(test_name);
        if (sr.tid == "ocen") {
            sr.gid = "0";
        }

        auto [it, _] = tests_groups.try_emplace(sr.gid);
        auto& group = it->second;
        group.tests.emplace(test_name, test);
    }

    // Load scoring
    const auto& scoring = sf.config["scoring"];
    if (not opts.ignore_simfile and not opts.reset_scoring and scoring.is_set() and
        scoring.is_array())
    {
        for (const auto& str : scoring.as_array()) {
            StringView gid;
            int64_t score = 0;
            try {
                std::tie(gid, score) = Simfile::parse_scoring_item(str);
            } catch (const std::exception& e) {
                report_.append(
                    "\033[1;35mwarning\033[m: \"scoring\": ignoring"
                    " unparsable item -> ",
                    e.what()
                );
                continue;
            }

            auto it = tests_groups.find(gid);
            if (it != tests_groups.end()) {
                it->second.score = score;
            } else {
                report_.append(
                    "\033[1;35mwarning\033[m: \"scoring\": ignoring"
                    " score of the group with id `",
                    gid,
                    "` because no test belongs to it"
                );
            }
        }
    }

    // Sum positive scores
    uint64_t max_score = 0;
    size_t unscored_tests = 0;
    for (const auto& [_, group] : tests_groups) {
        max_score += meta::max(group.score.value_or(0), 0);
        unscored_tests += not group.score.has_value();
    }

    // Distribute score among tests with no score set
    if (unscored_tests > 0) {
        // max_score now tells how much score we have to distribute
        max_score = (max_score > 100 ? 0 : 100 - max_score);

        // Group 0 always has score equal to 0
        if (auto it = tests_groups.find("0");
            it != tests_groups.end() and !it->second.score.has_value())
        {
            it->second.score = 0;
            --unscored_tests;
            report_.append("Auto-scoring: score of the group with id `", it->first, "` set to 0");
        }

        // Distribute scoring
        for (auto& [gid, group] : tests_groups) {
            if (not group.score.has_value()) {
                max_score -= (group.score = max_score / unscored_tests--).value();
                report_.append(
                    "Auto-scoring: score of the group with id `",
                    gid,
                    "` set to ",
                    group.score.value()
                );
            }
        }
    }

    bool run_main_solution = opts.reset_time_limits_using_main_solution;

    // Export tests to the Simfile
    sf.tgroups.clear();
    for (const auto& [_, group] : tests_groups) {
        Simfile::TestGroup tg;
        tg.score = group.score.value();
        for (const auto& [test_name, test] : group.tests) {
            run_main_solution |= not test.time_limit.has_value();

            Simfile::Test t(
                test_name.to_string(),
                test.time_limit.value_or(std::chrono::nanoseconds(0)),
                test.memory_limit.value()
            );
            t.in = test.in.value().to_string();
            if (sf.interactive) {
                t.out = std::nullopt;
            } else {
                t.out = test.out.value().to_string();
            }

            tg.tests.emplace_back(std::move(t));
        }

        sf.tgroups.emplace_back(std::move(tg));
    }

    // Override the time limits
    if (opts.global_time_limit.has_value()) {
        run_main_solution = opts.reset_time_limits_using_main_solution;
        for (auto&& g : sf.tgroups) {
            for (auto&& t : g.tests) {
                t.time_limit = opts.global_time_limit.value();
            }
        }
    }

    if (not run_main_solution) {
        normalize_time_limits(sf);
        // Nothing more to do
        return {Status::COMPLETE, std::move(sf), main_dir.to_string()};
    }

    // The main solution's judge report is needed => set the time limits for it
    auto sol_time_limit =
        std::chrono::duration_cast<std::chrono::milliseconds>(time_limit_to_solution_runtime(
            opts.max_time_limit,
            opts.rtl_opts.solution_runtime_coefficient,
            opts.rtl_opts.min_time_limit
        ));
    for (auto&& g : sf.tgroups) {
        for (auto&& t : g.tests) {
            t.time_limit = sol_time_limit;
        }
    }

    return {Status::NEED_MODEL_SOLUTION_JUDGE_REPORT, std::move(sf), main_dir.to_string()};
}

void Conver::reset_time_limits_using_jugde_reports(
    Simfile& sf,
    const JudgeReport& jrep1,
    const JudgeReport& jrep2,
    const ResetTimeLimitsOptions& opts
) {
    STACK_UNWINDING_MARK;

    using std::chrono::nanoseconds;
    using std::chrono_literals::operator""ns;

    if (opts.min_time_limit < 0ns) {
        THROW("min_time_limit has to be non-negative");
    }
    if (opts.solution_runtime_coefficient < 0) {
        THROW("solution_runtime_coefficient has to be non-negative");
    }

    // Map every test to its time limit
    std::map<StringView, nanoseconds> tls; // test name => time limit
    for (auto ptr : {&jrep1, &jrep2}) {
        auto&& rep = *ptr;
        for (auto&& g : rep.groups) {
            for (auto&& t : g.tests) {
                // Only allow OK and WA to pass through
                if (not is_one_of(t.status, JudgeReport::Test::OK, JudgeReport::Test::WA)) {
                    THROW(
                        "Error on test `", t.name, "`: ", JudgeReport::Test::description(t.status)
                    );
                }

                tls[t.name] = floor_to_10ms(solution_runtime_to_time_limit(
                    t.runtime, opts.solution_runtime_coefficient, opts.min_time_limit
                ));
            }
        }
    }

    // Assign time limits to the tests
    for (auto&& tg : sf.tgroups) {
        for (auto&& t : tg.tests) {
            auto it = tls.find(t.name);
            if (it != tls.end()) {
                // Reset time limit only on tests from judge reports
                t.time_limit = it->second;
            }
        }
    }

    normalize_time_limits(sf);
}

} // namespace sim
